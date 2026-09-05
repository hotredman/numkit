@echo off
rem Full from-scratch rebuild for Windows: wipe build dirs, build engine (windows-release),
rem optionally the WASM/browser stack, run the gtest suite, refresh npm dist.
rem
rem Usage:
rem   scripts\windows\rebuild-all.cmd            engine (windows-release) + tests
rem   scripts\windows\rebuild-all.cmd --wasm     + wasm-release and the IDE web bundle
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set "PRESETS=windows\release windows\debug windows\portable"
set "WASM=0"
for %%a in (%*) do (
    if "%%a"=="--wasm" set "WASM=1"
)

echo === rebuild-all: wiping build dirs ===
for %%p in (%PRESETS%) do (
    if exist "%PROJECT_DIR%build\%%p" (
        echo   rmdir build\%%p
        rmdir /s /q "%PROJECT_DIR%build\%%p"
    )
)
if "%WASM%"=="1" (
    if exist "%PROJECT_DIR%build\wasm\release" (
        echo   rmdir build\wasm\release
        rmdir /s /q "%PROJECT_DIR%build\wasm\release"
    )
)

echo.
echo === engine (windows-release) ===
call "%SCRIPT_DIR%engine-build.cmd"
if errorlevel 1 goto :fail

if "%WASM%"=="1" (
    echo.
    echo === wasm + web bundle ===
    call "%SCRIPT_DIR%web-build.cmd"
    if errorlevel 1 goto :fail
)

echo.
echo === gtest suite ===
set TEST_EXE=%PROJECT_DIR%build\windows\release\tests\gtest\Release\numkit_gtest.exe
if not exist "%TEST_EXE%" set TEST_EXE=%PROJECT_DIR%build\windows\release\tests\gtest\numkit_gtest.exe
"%TEST_EXE%"
if errorlevel 1 goto :fail

if "%WASM%"=="1" (
    echo.
    echo === npm dist refresh ===
    node "%PROJECT_DIR%packages\numkit\scripts\refresh-dist.js"
    if errorlevel 1 goto :fail
) else (
    echo === npm dist refresh: skipped ^(no --wasm; dist mirrors the wasm build^)
)

echo.
echo rebuild-all: OK
exit /b 0

:fail
echo.
echo rebuild-all: FAILED
exit /b 1
