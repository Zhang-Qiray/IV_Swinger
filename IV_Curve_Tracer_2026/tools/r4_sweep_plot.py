#!/usr/bin/env python
"""Simple UNO R4 SWEEP plotter.

This tool talks directly to the IV_Swinger2_R4 firmware over USB serial:

  1. Sends SWEEP by default, or SWEEP_T <points> <delay_us>
  2. Reads calibrated SWEEP output from the current firmware:
       Voc = <volts> Isc = <amps> Points = <count> us/Point = <microseconds>
       I=<amps> V=<volts>
       I = <amps> V = <volts>
  3. Saves raw text and CSV
  4. Plots the sweep curve

It is intentionally small and plain so it is easy to modify while bringing up
the R4 firmware.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import serial
from serial import SerialException
from serial.tools import list_ports


IV_PAIR_RE = re.compile(r"^I\s*=\s*(-?\d+(?:\.\d+)?)\s+V\s*=\s*(-?\d+(?:\.\d+)?)$")
POINTS_RE = re.compile(r"\bPoints\s*=\s*(\d+)\b")
VOC_ISC_RE = re.compile(
    r"\bVoc\s*=\s*(-?\d+(?:\.\d+)?)\s+Isc\s*=\s*(-?\d+(?:\.\d+)?)"
)
MPP_RE = re.compile(
    r"^MPP\s+P\s*=\s*(-?\d+(?:\.\d+)?)\s+I\s*=\s*(-?\d+(?:\.\d+)?)\s+V\s*=\s*(-?\d+(?:\.\d+)?)$"
)


@dataclass
class SweepPoint:
    index: int
    x: float
    y: float


def list_serial_ports() -> list[str]:
    ports = list(list_ports.comports())
    if ports:
        print("Available serial ports:")
        for item in ports:
            print(f"  {item.device:8s} {item.description}")
    return [item.device for item in ports]


def read_until_quiet(port: serial.Serial, quiet_s: float, max_s: float) -> list[str]:
    deadline = time.monotonic() + max_s
    quiet_deadline = time.monotonic() + quiet_s
    text = ""

    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1).decode("utf-8", errors="replace")
        if chunk:
            text += chunk
            quiet_deadline = time.monotonic() + quiet_s
        elif time.monotonic() >= quiet_deadline:
            break

    return [line.strip() for line in text.splitlines() if line.strip()]


def run_sweep(
    port_name: str,
    baud: int,
    command_name: str,
    points: int,
    delay_us: int,
    timeout: float,
    verbose_points: bool,
) -> tuple[list[str], list[SweepPoint]]:
    command_name = command_name.upper()
    if command_name == "SWEEP_T":
        command = f"{command_name} {points} {delay_us}"
    else:
        command = command_name

    print(f"Opening {port_name} at {baud} baud")
    with serial.Serial(port_name, baudrate=baud, timeout=0.05, write_timeout=1) as port:
        time.sleep(0.8)
        read_until_quiet(port, quiet_s=0.2, max_s=1.0)

        print(f"Sending: {command}")
        port.write((command + "\n").encode("ascii"))
        port.flush()

        lines: list[str] = []
        sweep_points: list[SweepPoint] = []
        expected_points = points
        start = time.monotonic()
        last_data_time = start
        pending_text = ""

        while time.monotonic() - start < timeout:
            chunk = port.read(port.in_waiting or 1).decode("utf-8", errors="replace")
            if not chunk:
                if sweep_points and time.monotonic() - last_data_time > 1.0:
                    return lines, sweep_points
                continue

            last_data_time = time.monotonic()
            pending_text += chunk
            while "\n" in pending_text:
                line, pending_text = pending_text.split("\n", 1)
                line = line.strip()
                if not line:
                    continue

                lines.append(line)
                point_was_read = False
                points_match = POINTS_RE.search(line)
                if points_match:
                    expected_points = int(points_match.group(1))

                iv_pair_match = IV_PAIR_RE.match(line)
                if iv_pair_match:
                    sweep_points.append(
                        SweepPoint(
                            index=len(sweep_points),
                            x=float(iv_pair_match.group(2)),
                            y=float(iv_pair_match.group(1)),
                        )
                    )
                    point_was_read = True

                if point_was_read:
                    if verbose_points:
                        print(line)
                    elif (
                        len(sweep_points) == 1
                        or len(sweep_points) % 500 == 0
                        or len(sweep_points) >= expected_points
                    ):
                        print(f"read_points={len(sweep_points)}/{expected_points}")
                else:
                    print(line)

                if line.startswith("ERR "):
                    return lines, sweep_points

                if line.startswith("END_SWEEP"):
                    return lines, sweep_points

        raise TimeoutError(
            f"Timed out after {timeout:.1f}s; read {len(sweep_points)} of {expected_points} points"
        )


def save_outputs(
    output_dir: Path,
    lines: list[str],
    points: list[SweepPoint],
    x_label: str,
    y_label: str,
) -> tuple[Path, Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    raw_path = output_dir / f"r4_sweep_{stamp}.txt"
    csv_path = output_dir / f"r4_sweep_{stamp}.csv"

    raw_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["index", x_label, y_label])
        for point in points:
            writer.writerow([point.index, point.x, point.y])

    return raw_path, csv_path


def trim_points(points: list[SweepPoint], skip_start: int) -> list[SweepPoint]:
    if skip_start <= 0:
        return points

    trimmed = points[min(skip_start, len(points)) :]
    return [SweepPoint(index=index, x=point.x, y=point.y) for index, point in enumerate(trimmed)]


def print_summary(lines: list[str], points: list[SweepPoint], x_label: str, y_label: str) -> None:
    print()
    print("Summary:")
    print(f"  points_read={len(points)}")

    for line in lines:
        voc_isc_match = VOC_ISC_RE.search(line)
        if voc_isc_match:
            print(f"  voc={float(voc_isc_match.group(1)):.4f} isc={float(voc_isc_match.group(2)):.4f}")
            points_match = POINTS_RE.search(line)
            if points_match:
                print(f"  firmware_points={points_match.group(1)}")
            break

    for line in lines:
        mpp_match = MPP_RE.match(line)
        if mpp_match:
            print(
                "  "
                f"mpp_watts={float(mpp_match.group(1)):.4f} "
                f"mpp_amps={float(mpp_match.group(2)):.4f} "
                f"mpp_volts={float(mpp_match.group(3)):.4f}"
            )
            break

    if not points:
        print("  no I = ... V = ... point lines found")
        return

    x_values = [point.x for point in points]
    y_values = [point.y for point in points]
    print(f"  {x_label}_first={x_values[0]:.4f} {x_label}_last={x_values[-1]:.4f} {x_label}_min={min(x_values):.4f} {x_label}_max={max(x_values):.4f}")
    print(f"  {y_label}_first={y_values[0]:.4f} {y_label}_last={y_values[-1]:.4f} {y_label}_min={min(y_values):.4f} {y_label}_max={max(y_values):.4f}")


def plot_points(points: list[SweepPoint], title: str, save_path: Path | None, x_label: str, y_label: str) -> None:
    if not points:
        raise ValueError("No points to plot")

    indexes = [point.index for point in points]
    x_values = [point.x for point in points]
    y_values = [point.y for point in points]

    fig, axes = plt.subplots(3, 1, figsize=(10, 10))
    fig.suptitle(title)

    axes[0].plot(indexes, x_values, label=x_label, color="tab:blue")
    axes[0].set_xlabel("Sample index")
    axes[0].set_ylabel(x_label)
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(indexes, y_values, label=y_label, color="tab:orange")
    axes[1].set_xlabel("Sample index")
    axes[1].set_ylabel(y_label)
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(x_values, y_values, marker=".", linewidth=1, color="tab:green")
    axes[2].set_xlabel(x_label)
    axes[2].set_ylabel(y_label)
    axes[2].set_title("I-V curve")
    axes[2].grid(True, alpha=0.3)

    fig.tight_layout()

    if save_path:
        save_path.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(save_path, dpi=140)
        print(f"Saved plot: {save_path}")

    plt.show()
    plt.close(fig)


def run_once(args: argparse.Namespace, points: int, delay_us: int) -> int:
    output_dir = Path(args.output_dir)
    command_name = args.command.upper()
    x_label = "volts"
    y_label = "amps"

    try:
        lines, sweep_points = run_sweep(
            args.port,
            args.baud,
            command_name,
            points,
            delay_us,
            args.timeout,
            args.verbose_points,
        )
    except SerialException as exc:
        print()
        print(f"Could not open {args.port}: {exc}")
        print("Close Arduino Serial Monitor, Serial Plotter, or any other program using this COM port.")
        return 2
    except TimeoutError as exc:
        print()
        print(f"Timeout: {exc}")
        print("Try fewer points, a longer --timeout, or check whether the firmware returned an ERR line.")
        return 3

    plot_points_data = trim_points(sweep_points, args.skip_start)
    if args.skip_start > 0:
        print(f"Skipped first {len(sweep_points) - len(plot_points_data)} point(s) for CSV/plot")

    raw_path, csv_path = save_outputs(output_dir, lines, plot_points_data, x_label, y_label)

    print_summary(lines, plot_points_data, x_label, y_label)
    print(f"Saved raw text: {raw_path}")
    print(f"Saved CSV: {csv_path}")

    if not plot_points_data:
        print("No points to plot; saved raw output for troubleshooting.")
        return 4

    if not args.no_plot:
        png_path = output_dir / (csv_path.stem + ".png")
        plot_points(
            plot_points_data,
            title=f"{command_name} skip_start={args.skip_start} on {args.port}",
            save_path=png_path,
            x_label=x_label,
            y_label=y_label,
        )

    return 0


def parse_loop_command(text: str, default_points: int, default_delay_us: int) -> tuple[int, int] | None:
    text = text.strip().lstrip("\ufeff")
    if not text:
        raise ValueError("empty")

    if text.lower() in {"q", "quit", "exit"}:
        return None

    if text.lower() in {"r", "repeat"}:
        return default_points, default_delay_us

    parts = text.replace(",", " ").split()
    if len(parts) > 2:
        raise ValueError("Please enter: points delay_us")

    points = int(parts[0])
    delay_us = default_delay_us
    if len(parts) == 2:
        delay_us = int(parts[1])

    return points, delay_us


def parse_loop_skip_start(text: str, default_skip_start: int) -> int | None:
    text = text.strip().lstrip("\ufeff")
    if not text:
        raise ValueError("empty")

    if text.lower() in {"q", "quit", "exit"}:
        return None

    if text.lower() in {"r", "repeat"}:
        return default_skip_start

    parts = text.replace(",", " ").split()
    if len(parts) != 1:
        raise ValueError("Please enter: skip_start")

    return int(parts[0])


def run_loop(args: argparse.Namespace) -> int:
    points = args.points
    delay_us = args.delay_us
    skip_start = args.skip_start

    print()
    print(f"Interactive {args.command.upper()} mode")
    if args.command.upper() == "SWEEP_T":
        print("  Enter: points delay_us")
        print("  Example: 1200 20")
    else:
        print("  Enter: skip_start")
        print("  Example: 10")
    print("  Enter r to repeat the previous values.")
    print("  Press Enter without text to do nothing.")
    print("  Enter q to quit.")

    while True:
        print()
        try:
            if args.command.upper() == "SWEEP_T":
                prompt = f"sweep [{points} {delay_us}]> "
            else:
                prompt = f"sweep [skip_start={skip_start}]> "
            text = input(prompt)
        except EOFError:
            print()
            print("Done.")
            return 0

        try:
            if args.command.upper() == "SWEEP_T":
                parsed = parse_loop_command(text, points, delay_us)
            else:
                parsed = parse_loop_skip_start(text, skip_start)
        except ValueError as exc:
            if str(exc) == "empty":
                continue
            print(f"Bad input: {exc}")
            continue

        if parsed is None:
            print("Done.")
            return 0

        if args.command.upper() == "SWEEP_T":
            points, delay_us = parsed
        else:
            skip_start = parsed
            args.skip_start = skip_start

        if points < 1:
            print("Bad input: points must be >= 1")
            continue
        if args.command.upper() == "SWEEP_T" and delay_us < 0:
            print("Bad input: delay_us must be >= 0")
            continue
        if args.command.upper() != "SWEEP_T" and skip_start < 0:
            print("Bad input: skip_start must be >= 0")
            continue

        status = run_once(args, points, delay_us)
        if status == 2:
            return status


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Run SWEEP and plot I-V points")
    parser.add_argument("--port", default="COM3", help="Serial port, for example COM3")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--command", default="SWEEP", choices=["SWEEP", "SWEEP_T"], help="Firmware command to run")
    parser.add_argument("--points", type=int, default=2500, help="SWEEP_T point count, or SWEEP progress upper limit")
    parser.add_argument("--delay-us", type=int, default=0, help="SWEEP_T delay after each sample pair")
    parser.add_argument("--timeout", type=float, default=20.0, help="Read timeout in seconds")
    parser.add_argument("--output-dir", default="sweep_logs", help="Folder for TXT/CSV/PNG outputs")
    parser.add_argument("--no-plot", action="store_true", help="Save files but do not open a plot window")
    parser.add_argument("--skip-start", type=int, default=0, help="Drop this many leading points from CSV and plot")
    parser.add_argument("--verbose-points", action="store_true", help="Print every ADC point while reading")
    parser.add_argument("--loop", action="store_true", help="Keep asking for new points/delay_us values")
    parser.add_argument("--list-ports", action="store_true", help="List serial ports and exit")
    args = parser.parse_args(argv)

    ports = list_serial_ports()
    if args.list_ports:
        return 0

    if args.port not in ports:
        print(f"Warning: {args.port} was not found in the current port list.")

    if args.loop:
        return run_loop(args)

    return run_once(args, args.points, args.delay_us)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
