@echo off
setlocal

:: benchmarks/bench_simd.bat — SIMD speedup runner
::
:: Runs every SIMD-optimised kernel on both the scalar-baseline build
:: (preset=bench) and the Highway-SIMD build (preset=bench-simd), then
:: side-by-sides the numbers via Google Benchmark's compare.py if
:: Python is on PATH.
::
:: Kernels covered (Phase 8a-e):
::   BM_Abs            abs                 (8a)
::   BM_Sin/Cos/Exp/Log sin cos exp log    (8b)
::   BM_Plus/Times     plus times          (8c)
::   BM_Mtimes_Square  mtimes / matmul     (8d)
::   BM_Fft_PowerOfTwo fft                 (8e)
::
:: Prereq:
::   cmake --preset=bench      && cmake --build --preset=bench
::   cmake --preset=bench-simd && cmake --build --preset=bench-simd

set PROJECT_DIR=%~dp0..\
set FILTER=BM_Abs^|BM_Sin^|BM_Cos^|BM_Exp^|BM_Log^|BM_Plus^|BM_Times^|BM_Mtimes_Square^|BM_Fft_PowerOfTwo
set PORTABLE=%PROJECT_DIR%build-bench\benchmarks\Release\m_bench.exe
set SIMD=%PROJECT_DIR%build-bench-simd\benchmarks\Release\m_bench.exe
set COMPARE=%~dp0compare_simd.py
set BASELINE_JSON=%~dp0baseline.json
set SIMD_JSON=%~dp0simd.json
set MIN_TIME=0.2s

if not exist "%PORTABLE%" (
    echo.
    echo Portable bench exe not found: %PORTABLE%
    echo Build the scalar baseline first:
    echo   cmake --preset=bench ^&^& cmake --build --preset=bench
    exit /b 1
)
if not exist "%SIMD%" (
    echo.
    echo SIMD bench exe not found: %SIMD%
    echo Build the Highway-SIMD variant first:
    echo   cmake --preset=bench-simd ^&^& cmake --build --preset=bench-simd
    exit /b 1
)

echo.
echo [1/2] Portable ^(scalar baseline^)...
"%PORTABLE%" --benchmark_filter="%FILTER%" --benchmark_min_time=%MIN_TIME% --benchmark_out="%BASELINE_JSON%" --benchmark_out_format=json --benchmark_format=console
if errorlevel 1 exit /b 1

echo.
echo [2/2] Desktop-fast ^(Highway SIMD^)...
"%SIMD%" --benchmark_filter="%FILTER%" --benchmark_min_time=%MIN_TIME% --benchmark_out="%SIMD_JSON%" --benchmark_out_format=json --benchmark_format=console
if errorlevel 1 exit /b 1

echo.
where python >nul 2>&1
if errorlevel 1 (
    echo Python not on PATH — skipping compare.py.
    echo Raw JSON results saved next to this script:
    echo   %BASELINE_JSON%
    echo   %SIMD_JSON%
    exit /b 0
)

echo === Speedup table ^(portable vs. desktop-fast^) ===
python "%COMPARE%" "%BASELINE_JSON%" "%SIMD_JSON%"
