# IV_Swinger

**NOTICE: Please contact me at csatt1@gmail.com if you are interested in building the hardware for people who don't have the time, skills, or tools to do it themselves. I get requests occasionally from such people, but cannot do this myself. There are now PCBs, making the construction much easier!**

The IV Swinger is an IV curve tracer for photovoltaic solar panels. Both the hardware design and the software are open source.

This GitHub repository contains the IV Swinger documentation, software, and open source licensing files.

IV Swinger 2 is here! It is much smaller, cheaper and easier to build, and better than the original in almost every way. Step-by-step construction instructions are included in the repository, and are posted on www.instructables.com:

[IV Swinger 2 - a $50 IV Curve Tracer](http://www.instructables.com/id/IV-Swinger-2-a-50-IV-Curve-Tracer/)

All IV Swinger 2 documentation is now available:
* IV Swinger 2: User Guide
* IV Swinger 2: Optional Environmental Sensors
* IV Swinger 2: Hardware Scaling
* IV Swinger 2: Design and Theory of Operation
* IV Swinger 2: Step-by-step Construction (for each variant)
* Fritzing files
* PCB schematics
* Bills of materials (BOMs) for each variant

YouTube demo videos (IV Swinger 2):

[![IV Swinger 2 Demo: Part I](http://img.youtube.com/vi/WhnTWciiNNo/0.jpg)](http://www.youtube.com/watch?v=WhnTWciiNNo)

[![IV Swinger 2 Demo: Part II](http://img.youtube.com/vi/9iPq5AsuU_U/0.jpg)](http://www.youtube.com/watch?v=9iPq5AsuU_U)

YouTube demo video (original IV Swinger):

[![IV Swinger Demo](http://img.youtube.com/vi/xNytkONOcW0/0.jpg)](http://www.youtube.com/watch?v=xNytkONOcW0)

## Arduino UNO R4 WiFi teaching firmware

The simplified classroom firmware is in:

```text
IV_Curve_Tracer_2026/firmware/IV_Swinger2_R4
```

This version is organized for two teaching goals:

1. One-command IV curve capture with automatic scan timing.
2. Manual scan experiments where students choose the point count and sample
   delay, then observe how those parameters change the measured curve.

### Student commands

These are the commands normally used during a lab:

| Command | Purpose |
| --- | --- |
| `SWEEP` | Automatic IV scan. The firmware prescans the panel, targets 2500 useful points, and keeps 100 extra buffer points in reserve. |
| `SWEEP_T <points> <sample_delay_us>` | Teaching scan. Students choose the number of points and the delay after each ADC pair. |
| `VOC` | Measure open-circuit voltage using the fixed firmware sample count. |
| `ISC` | Measure short-circuit current using the fixed firmware sample count. |
| `STATE` | Show current firmware settings. |
| `IDLE` | Return relays to the idle safe state. |
| `HELP` | Show student commands. |
| `HELP ALL` | Show student and debug commands. |

Examples:

```text
SWEEP
SWEEP_T 1200 20
SWEEP_T 2500 0
VOC
ISC
```

### Output format

`SWEEP` and `SWEEP_T` use the same plotting format:

```text
Voc = <volts> Isc = <amps> Points = <count> us/Point = <microseconds>
MPP P = <watts> I = <amps> V = <volts>
I = <amps> V = <volts>
I = <amps> V = <volts>
...
```

This keeps the Python plotter simple: automatic and manual scans can be plotted
with the same parser.

### Debug commands

The older duplicate commands were removed. The remaining debug commands are:

| Command | Purpose |
| --- | --- |
| `PING` | Quick serial communication check. |
| `RELAY <1|2|3> <ON|OFF|1|0>` | Manually switch SSR outputs. |
| `ADC <0|1> [count]` | Read one ADC channel. |
| `ADC_BOTH [count]` | Read voltage and current ADC channels together. |
| `ADC_SPEED [count]` | Measure ADC pair-read speed. |
| `SPI_HZ <1000000..4000000>` | Set MCP3202 SPI speed. |
| `CAL` | Print calibration constants. |
| `SET_CAL <name> <value>` | Change a calibration constant. |
| `VOC_TEST [count]` | Detailed Voc and current-channel noise check. |
| `ISC_TEST [samples] [settle_ms]` | Detailed short-circuit current test. |
| `SWEEP_ALL` | Automatic sweep with readable debug summary. |
| `SWEEP_POINTS` | Detailed CSV-style data from the last formal sweep. |
| `STOP` | Stop and return to idle relay state. |

Removed legacy commands include `START_SWEEP`, `RAW_SWEEP_TEST`, `RAW_DUMP`,
`ADC_STATS`, `VOC_ISC_TEST`, `ISC_PATH`, and `SWEEP_PATH`.

### Python plotting

Use the helper script from the R4 tools folder:

```bat
cd IV_Curve_Tracer_2026\tools
plot_r4_sweep.bat
```

Choose:

* `A` for automatic `SWEEP`
* `T` for teaching `SWEEP_T`

In teaching mode, enter different `points delay_us` values without restarting
the script, for example:

```text
1200 20
2500 0
800 50
```

The lower-level command is:

```bat
python r4_sweep_plot.py --port COM3 --command SWEEP_T --points 1200 --delay-us 20 --loop
```

### Code reading order

For normal study and modification, start with these files:

1. `IV_Swinger2_R4.ino` - setup, loop, and reading order.
2. `2_Config.ino` - pin map, main constants, calibration values, shared state.
3. `5_SWEEP.ino` - public `VOC`, `ISC`, `SWEEP`, and `SWEEP_T` commands.
4. `5a_Measure.ino` - shared `VOC` / `ISC` measurement helpers.
5. `5b_AutoSweep.ino` - automatic `SWEEP` scan flow.
6. `5c_Sweep_manually.ino` - manual teaching `SWEEP_T` scan flow.
7. `5d_Output.ino` - final IV output format and ADC conversion.
8. `6_Protocol.ino` - line-based serial input and HELP text.
9. `7_Protocol_Commands.ino` - daily-use command handlers.
10. `Debugging.ino` - debug tools; skip this during normal reading.
