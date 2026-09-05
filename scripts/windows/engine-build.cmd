@echo off
setlocal

cd /d "%~dp0..\.."
set PROJECT_DIR=%CD%

if not defined EMSDK (
    if exist "%USERPROFILE%\Repo\emsdk" set "EMSDK=%USERPROFILE%\Repo\emsdk"
    if exist "%USERPROFILE%\emsdk" set "EMSDK=%USERPROFILE%\emsdk"
)
set EMCC_DIR=%EMSDK%\upstream\emscripten

if "%1"=="--wasm" goto wasm
if "%1"=="--debug" goto debug
if "%1"=="--portable" goto portable
goto release

:release
:: Standard native build with Highway SIMD and Threads
cmake --preset=windows-release
if errorlevel 1 exit /b 1
cmake --build --preset=windows-release
if errorlevel 1 exit /b 1
echo Build OK (windows-release)
goto end

:debug
:: Debug build with symbols
cmake --preset=windows-debug
if errorlevel 1 exit /b 1
cmake --build --preset=windows-debug
if errorlevel 1 exit /b 1
echo Build OK (windows-debug)
goto end

:portable
:: Scalar reference build (no SIMD)
cmake --preset=windows-portable
if errorlevel 1 exit /b 1
cmake --build --preset=windows-portable
if errorlevel 1 exit /b 1
echo Build OK (windows-portable)
goto end

:wasm
:: WASM build via 'wasm-release' preset
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

cmake --preset=wasm-release
if errorlevel 1 exit /b 1
cmake --build --preset=wasm-release
if errorlevel 1 exit /b 1
echo Build OK (wasm-release)
goto end

:end
exit /b 0
