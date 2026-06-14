# IV Swinger2 R4 Firmware

This folder contains the Arduino UNO R4 WiFi / Minima firmware for an IV Swinger2-style I-V curve tracer.

The current goal is to make the firmware useful as a teaching lab module:

1. Students can generate an I-V curve with one command.
2. Teachers and students can change sampling parameters and observe how point count, scan speed, and scan completeness relate to the physical capacitor-charging process.
3. ADC, relay, calibration, and raw-sweep debug tools are still available, but they are hidden from the default student help view.

## Current Status

The following core command path has been tested:

```text
SWEEP
SWEEP_T <points> <sample_delay_us>
VOC
ISC
STATE
HELP
-------------------------------------------------
PING
RELAY
ADC
ADC_BOTH
ADC_SPEED
SPI_HZ
CAL
SET_CAL
VOC_TEST
ISC_TEST
SWEEP_ALL
SWEEP_POINTS
STOP
```



## File Layout

This project intentionally uses multiple `.ino` tabs instead of `.h` / `.cpp` files.

Each `.ino` file appears as a tab in Arduino IDE, which makes the firmware easier to read and modify during lab development.

Suggested reading order:

| File | Purpose |
| --- | --- |
| `IV_Swinger2_R4.ino` | Main entry point, only `setup()` and `loop()` |
| `2_Config.ino` | UNO R4 pin map, defaults, calibration values, shared sweep data |
| `3_ADC.ino` | MCP3202 ADC access |
| `4_Relay.ino` | SSR relay control and safe states |
| `5_SWEEP.ino` | Public `VOC`, `ISC`, `SWEEP`, and `SWEEP_T` command entry points |
| `5a_Measure.ino` | Shared `VOC` / `ISC` measurement helpers |
| `5b_AutoSweep.ino` | Automatic `SWEEP` / `SWEEP_ALL` scan flow |
| `5c_Sweep_manually.ino` | Manual teaching `SWEEP_T` scan flow |
| `5d_Output.ino` | Sweep output and ADC-to-volts/amps conversion |
| `6_Protocol.ino` | Line-based serial input, `HELP`, `STATE` |
| `7_Protocol_Commands.ino` | Serial command handlers and argument parsing |
| `Debugging.ino` | Teacher/debug tools |

For the formal scan logic, start with:

```text
5_SWEEP.ino
5a_Measure.ino
5b_AutoSweep.ino
5d_Output.ino
```

For the serial command path, start with:

```text
6_Protocol.ino
7_Protocol_Commands.ino
```

## Hardware Target

The firmware follows the IV Swinger2 SSR-module style wiring.

| UNO R4 pin | Function |
| --- | --- |
| D2 | SSR1, main PV-to-capacitor scan switch |
| D6 | SSR2, capacitor bleed/discharge path |
| D7 | SSR3, Isc bypass path |
| D10 | MCP3202 ADC chip select |
| D11 | SPI MOSI |
| D12 | SPI MISO |
| D13 | SPI SCK |

ADC channel convention:

```text
CH0 = voltage channel
CH1 = current channel
```

Safety note:

```text
After a real PV panel is connected, avoid manually running RELAY unless you are intentionally debugging hardware.
For normal measurements, use VOC, ISC, SWEEP, and SWEEP_ALL.
```

## Serial Settings

```text
Baud rate: 115200
Line ending: Newline, or Both NL & CR
```

Expected boot message:

```text
Ready!
OK BOOT name=IV_Swinger2_R4 hardware=SSR_MODULE protocol=simple_text baud=115200
OK INFO send HELP for student commands, HELP ALL for debug commands
```

## Command Layers

`HELP` shows only student-facing commands:

```text
HELP
```

Example:

```text
GROUP student
CMD SWEEP  -> auto IV curve, target_points=2500
NOTE SWEEP reserve_points=100
CMD VOC  -> open-circuit voltage, samples=500
CMD ISC  -> short-circuit current, samples=500
CMD STATE
CMD IDLE
CMD HELP ALL  -> show teacher/debug commands
```

Use `HELP ALL` to show teacher and debug commands:

```text
HELP ALL
```

This keeps the student interface simple without removing any development tools.

## Student Commands

### `VOC`

Measure open-circuit voltage. Output is a single number in volts.

```text
VOC
40.7603
```

### `ISC`

Measure short-circuit current. Output is a single number in amps.

```text
ISC
3.6786
```

If the current does not become stable:

```text
ERR ISC not_stable
```

### `SWEEP`

Run an automatic formal I-V sweep.

`SWEEP` does not take a point-count argument. It targets `MAX_IV_POINTS = 2500` plotted points and keeps 100 extra buffer points in reserve in case the formal sweep is slightly slower than the prescan.

The first line reports the measured endpoints and output point count:

```text
SWEEP
Voc = 40.7603 Isc = 3.6786 Points = 987 us/Point = 30.85
MPP P = 80.1234 I = 2.1234 V = 37.7270
I = 3.7130 V = 0.3265
I = 3.7130 V = 0.2763
I = 3.7068 V = 0.2511
...
I = 0.0470 V = 40.4589
```

Each point line is:

```text
I=<amps> V=<volts>
```

`SWEEP` outputs only real points captured during the sweep. It does not manually add artificial Isc or Voc endpoints.

### `STATE`

Show runtime state:

```text
STATE
STATE spi_hz=4000000 min_isc_adc=100 isc_stable_adc=5 adc_ref=4.8800 v_scale=1.000000 i_scale=1.000000
RELAY_STATE ssr1=OFF ssr2=ON ssr3=OFF
```

### `IDLE`

Return to the safe idle/discharge state.

```text
IDLE
```

## Teacher And Debug Commands

### `SWEEP_ALL`

Run the automatic formal sweep but print only a human-readable summary.

Use this to check tail detection, point count, speed, and the selected save strategy.

```text
SWEEP_ALL
OK SWEEP_ALL begin
SWEEP_METHOD auto_prescan_save_all_or_time_interval
PRESCAN
  complete=1 current_tail=1 isc_stable=1
  measured=1173 elapsed_us=30990 us_per_raw=26.42
  voc_adc=1622 isc_adc=1192 done_i_adc=20 done_delta_adc=3
SAVE_MODE
  mode=all_raw
  save_interval_us=26
FORMAL
  complete=1 current_tail=1 saved=987 target=2500 measured=997
  save_all_raw=1 timeout_check_every=16
  elapsed_us=30756 us_per_raw=30.85 raw_per_sec=32416.4
  complete=1
END_SWEEP_ALL status=ok
```

Important fields:

| Field | Meaning |
| --- | --- |
| `current_tail=1` | Current-channel raw ADC reached the original tail condition |
| `complete=1` | The sweep reached the current-tail stop condition |
| `isc_stable=1` | Isc was stable before the sweep |
| `done_i_adc` | Tail current threshold: `max(noise_floor * 2, 20)` raw ADC counts |
| `done_delta_adc` | Tail current delta threshold; original value is 3 raw ADC counts |
| `measured` | Raw ADC point count |
| `saved` | Points saved for output |
| `target` | Maximum save limit for this pass; normal target is 2500 plus a 100-point reserve |
| `elapsed_us` | Sweep duration in microseconds |
| `us_per_raw` | Average time per raw point |
| `raw_per_sec` | Raw point rate |
| `mode=all_raw` | All raw points fit in the output buffer |
| `mode=time_interval` | Raw points exceeded 2500, so points were saved by time interval |
| `save_interval_us` | Time spacing used when saving by interval |
| `timeout_check_every` | How often the loop checks the sweep timeout |

### `SWEEP_POINTS`

Print detailed CSV data from the last successful `SWEEP` or `SWEEP_ALL`.

```text
SWEEP_POINTS
BEGIN_SWEEP_POINTS format="index,volts,amps,adc_v,adc_i_raw,adc_i_corr"
0,0.3265,3.7130,13,1186,1185
1,0.2763,3.7130,11,1184,1185
...
END_SWEEP_POINTS
```

Each row is:

```text
index, volts, amps, voltage_adc, current_adc_raw, current_adc_corrected
```

If no successful sweep has been saved:

```text
ERR SWEEP_POINTS no_sweep_data
```

### `CAL`

Show conversion parameters:

```text
CAL
```

Important current defaults:

```text
adc_ref = 4.880
v_scale = 1.0000
i_scale = 1.0
```

### `SET_CAL <name> <value>`

Temporarily change one calibration value:

```text
SET_CAL adc_ref 4.880
SET_CAL v_scale 1.0000
SET_CAL i_scale 1.0000
```

These values are runtime-only for now. After reset or power loss, the firmware uses the defaults in `2_Config.ino` again.

### `ADC_BOTH [count]`

Read both ADC channels:

```text
ADC_BOTH 20
ADC_BOTH n=20 ch0_min=1617 ch0_avg=1619.60 ch0_max=1623 ch0_sat=0 ch1_min=0 ch1_avg=0.00 ch1_max=0 ch1_sat=0
```

### `ADC_SPEED [count]`

Measure ADC read speed:

```text
ADC_SPEED 500
ADC_SPEED n=500 elapsed_us=13030 us_per_pair=26.06 pairs_per_sec=38373.0 channel_reads_per_sec=76746.0 last_ch0=1618 last_ch1=0
```

### Manual Relay Commands

These commands are for hardware debugging:

```text
RELAY <1|2|3> <ON|OFF>
STOP
```

When a real PV panel is connected, prefer `VOC`, `ISC`, `SWEEP`, and `SWEEP_ALL`.

## Formal SWEEP Flow

The current `SWEEP` uses two passes:

```text
1. prescanSweep()
   Quickly scans once to measure the real raw point count and duration.

2. formalSweep()
   Scans again and saves points into the shared buffer.

3. output
   SWEEP prints Voc/Isc/Points, then I=<amps> V=<volts>.
   SWEEP_ALL prints a readable summary.
   SWEEP_POINTS prints detailed CSV points.
```

Save strategy:

```text
If prescan measured points fit inside the 2500-point target:
  mode = all_raw
  keep every real point

If prescan measured points exceed 2500:
  mode = time_interval
  read ADC as fast as possible, but save by prescan-based time interval
```

The formal sweep may save up to 2600 points. The extra 100 points are reserve
space for runs where the second sweep takes a little longer than the prescan.

`SWEEP` does not add artificial Isc or Voc points. The plotted data represents the real points captured during that sweep.

## Python Plotter

Use the helper from the R4 tools folder:

```bat
cd IV_Curve_Tracer_2026\tools
plot_r4_sweep.bat
```

The lower-level command is:

```bat
python r4_sweep_plot.py --port COM3 --command SWEEP_T --points 1200 --delay-us 20 --loop
```

```text
SWEEP
```

Outputs are saved under:

```text
sweep_logs\*.txt
sweep_logs\*.csv
sweep_logs\*.png
```

Interactive example:

```text
sweep [skip_start=0]> 10
```

Meaning:

```text
skip_start = 10
```

`skip_start` affects only the Python CSV and plot. It hides a few leading transient points without changing the Arduino sweep data.

## Suggested Lab Flow

For a student lab:

```text
1. STATE
2. VOC
3. ISC
4. SWEEP
5. Plot the curve with Python
6. Change skip_start or lab conditions and compare curves
7. Use SWEEP_ALL to inspect timing, real point count, and tail detection
```

Things students can observe:

```text
2500 is the normal target point count, not a guaranteed point count. The firmware can save up to 2600 points when the 100-point reserve is needed.
Scan speed depends on ADC speed, capacitor charging speed, and relay state.
At the end of the sweep, the current-channel raw ADC must fall below max(noise_floor * 2, 20) with less than 3 counts of change from the previous point.
```

## Troubleshooting

### Upload Fails

The COM port is usually busy.

Close:

```text
Arduino Serial Monitor
Python plot window
Other serial terminals
```

Then upload again.

### `SWEEP_ALL` Reports `prescan_isc_not_stable`

The firmware did not detect stable short-circuit current before the sweep.

Check:

```text
PV panel connection
Current-channel ADC reading
SSR3 / Isc path wiring
min_isc_adc threshold
```

### `SWEEP_POINTS` Reports `no_sweep_data`

No successful `SWEEP` or `SWEEP_ALL` has been run yet.

Run:

```text
SWEEP_ALL
```

or:

```text
SWEEP
```

### `SWEEP` Outputs Fewer Than 2500 Points

This is normal.

2500 is the normal target output count, not a guaranteed output count. The firmware has a 2600-point buffer, with the last 100 points reserved for sweep-to-sweep timing variation. The real point count depends on:

```text
PV panel condition
Capacitor charging speed
ADC read speed
Sweep end condition
Save strategy
```

Use `SWEEP_ALL` to inspect the measured raw point count, elapsed time, and save mode.

### First Points Are Noisy

Common causes:

```text
SSR switching transient
Capacitor beginning to charge
ADC quantization noise
Small time offset between current and voltage reads
```

For plotting, use Python `skip_start`, for example:

```text
10
```

### Sweep Does Not Reach Current Tail

The automatic sweep ends when the current-channel raw ADC is near the measured
noise floor and is no longer falling quickly.

If the current channel remains above the noise-floor tail threshold, the sweep
may run until the timeout instead of ending early. This is acceptable for
`SWEEP_T` experiments because the requested point count and delay are the main
teaching variables.

```text
current-channel raw ADC < max(noise_floor * 2, 20)
previous-current minus current < 3
```

## Suggested Next Steps

```text
1. Build a Python student menu: one-click curve, parameter experiment, curve comparison.
2. Add a SWEEP_EXP points delay_us teaching command.
3. Plot both I-V and P-V curves.
4. Automatically calculate Voc, Isc, Pmax, Vmp, and Imp.
```
