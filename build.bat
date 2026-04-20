@echo off
setlocal

set PROJECT_DIR=%~dp0
set EMSDK=C:\Users\User\Repo\emsdk
set EMCC_DIR=%EMSDK%\upstream\emscripten

if "%1"=="--wasm" goto wasm
if "%1"=="--fast" goto fast
goto default

:default
:: Native build via 'portable' preset (baseline, all NUMKIT_WITH_* OFF)
cmake --preset=portable
if errorlevel 1 exit /b 1
cmake --build --preset=portable
if errorlevel 1 exit /b 1
echo Build OK (portable)
goto end

:fast
:: Native build via 'desktop-fast' preset (Highway SIMD, pocketfft, ... as they come online)
cmake --preset=desktop-fast
if errorlevel 1 exit /b 1
cmake --build --preset=desktop-fast
if errorlevel 1 exit /b 1
echo Build OK (desktop-fast)
goto end

:wasm
:: WASM build via 'browser' preset
where ninja >nul 2>&1
if errorlevel 1 (
    if exist "%USERPROFILE%\bin\ninja.exe" (
        set "PATH=%USERPROFILE%\bin;%PATH%"
    ) else (
        echo ninja not found. Install from https://github.com/ninja-build/ninja/releases
        exit /b 1
    )
)

if not exist "%EMCC_DIR%\emcc.bat" (
    echo Emscripten not found at %EMCC_DIR%
    echo Install: cd %EMSDK% ^& emsdk install latest ^& emsdk activate latest
    exit /b 1
)

set "PATH=%EMCC_DIR%;%EMSDK%;%PATH%"
set "EM_CONFIG=%EMSDK%\.emscripten"

echo Configuring WASM build...
cmake --preset=browser
if errorlevel 1 exit /b 1

echo Building...
cmake --build --preset=browser
if errorlevel 1 exit /b 1

echo WASM build OK
echo Output: build-browser\wasm\dist\numkit_mide.js
echo         build-browser\wasm\dist\numkit_mide.wasm
goto end

:end
