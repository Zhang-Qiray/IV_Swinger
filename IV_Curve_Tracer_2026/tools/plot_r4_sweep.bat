@echo off
setlocal
cd /d "%~dp0"
set PYTHON_EXE=python
if exist ".venv\Scripts\python.exe" set PYTHON_EXE=.venv\Scripts\python.exe
set PORT=COM3
set POINTS=2500
echo.
echo IV Swinger2 R4 SWEEP plotter
echo.
set /p PORT=Serial port [%PORT%]: 
if "%PORT%"=="" set PORT=COM3
echo.
echo Mode:
echo   A = auto SWEEP
echo   T = teaching SWEEP_T points delay_us
set MODE=A
set /p MODE=Mode [%MODE%]: 
if "%MODE%"=="" set MODE=A
echo.
if /I "%MODE%"=="T" (
  "%PYTHON_EXE%" r4_sweep_plot.py --port "%PORT%" --command SWEEP_T --points %POINTS% --loop
) else (
  "%PYTHON_EXE%" r4_sweep_plot.py --port "%PORT%" --command SWEEP --points %POINTS% --loop
)
pause
