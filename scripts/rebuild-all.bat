@echo off
rem Full from-scratch rebuild: wipe build dirs, build the engine (desktop-fast),
rem optionally the WASM/browser stack, run the gtest suite, refresh the npm dist.
rem
rem Usage:
rem   scripts\rebuild-all.bat            engine (desktop-fast) + tests + dist
rem   scripts\rebuild-all.bat --wasm     + browser preset and the IDE web bundle
rem                                     (needs EMSDK; auto-detected like web-build.bat)
setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0..\
set "PRESETS=desktop-fast"
set "WASM=0"
for %%a in (%*) do (
    if "%%a"=="--wasm" set "WASM=1"
)

echo === rebuild-all: wiping build dirs ===
for %%p in (%PRESETS%) do (
    if exist "%PROJECT_DIR%\build\%%p" (
        echo   rmdir build\%%p
        rmdir /s /q "%PROJECT_DIR%\build\%%p"
    )
)
if "%WASM%"=="1" (
    if exist "%PROJECT_DIR%\build\browser" (
        echo   rmdir build\browser
        rmdir /s /q "%PROJECT_DIR%\build\browser"
    )
)

echo === engine (desktop-fast) ===
call "%~dp0engine-build.bat" --fast
if errorlevel 1 goto :fail

if "%WASM%"=="1" (
    echo === wasm + web bundle ===
    call "%~dp0web-build.bat"
    if errorlevel 1 goto :fail
)

echo === gtest suite ===
"%PROJECT_DIR%\build\desktop-fast\tests\gtest\Release\numkit_gtest.exe"
if errorlevel 1 goto :fail

rem The npm dist IS the wasm build — refresh it only when --wasm rebuilt it;
rem without a fresh wasm, refresh-dist correctly fails closed on staleness.
if "%WASM%"=="1" (
    echo === npm dist refresh ===
    node "%PROJECT_DIR%\packages\numkit\scripts\refresh-dist.js"
    if errorlevel 1 goto :fail
) else (
    echo === npm dist refresh: skipped ^(no --wasm; dist mirrors the wasm build^)
)

echo.
echo rebuild-all: OK
exit /b 0

:fail
echo.
echo rebuild-all: FAILED (see output above)
exit /b 1
