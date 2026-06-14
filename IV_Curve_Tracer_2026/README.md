# IV Curve Tracer 2026

This folder contains the active Arduino R4 teaching version of the IV curve
tracer and its companion plotting tools.

## Folder Layout

```text
firmware/IV_Swinger2_R4/  Arduino UNO R4 firmware. Open this folder in Arduino IDE.
tools/                    Python serial capture and plotting helper.
reference/                R4 board, MCU, and schematic reference PDFs.
```

`firmware/IV_Swinger2_R4/V1.zip` is the earliest runnable R4 code backup and is
kept for recovery.

## Quick Start

1. Upload `firmware/IV_Swinger2_R4/IV_Swinger2_R4.ino` with Arduino IDE.
2. Close Arduino Serial Monitor so Python can use the COM port.
3. Run:

```bat
cd IV_Curve_Tracer_2026\tools
plot_r4_sweep.bat
```

The current student commands are `SWEEP`, `SWEEP_T <points> <sample_delay_us>`,
`VOC`, `ISC`, `STATE`, `IDLE`, `HELP`, and `HELP ALL`.
