@echo off
setlocal

set PROJECT_DIR=%~dp0
set EMSDK=C:\Users\User\Repo\emsdk
set EMCC_DIR=%EMSDK%\upstream\emscripten

if "%1"=="--wasm" goto wasm
if "%1"=="--msvc" goto msvc
goto default

:default
:: Native build with Visual Studio
cmake -B build_vs -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build build_vs --config Release
if errorlevel 1 exit /b 1
echo Build OK
goto end

:msvc
:: Same as default, explicit flag
cmake -B build_vs -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build build_vs --config Release
if errorlevel 1 exit /b 1
echo Build OK (MSVC)
goto end

:wasm
:: WASM build with Emscripten + Ninja
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
call emcmake cmake -B build-wasm -DMLAB_BUILD_REPL=ON -G Ninja
if errorlevel 1 exit /b 1

echo Building...
cmake --build build-wasm
if errorlevel 1 exit /b 1

echo WASM build OK
echo Output: build-wasm\wasm\dist\mlab_repl.js
echo         build-wasm\wasm\dist\mlab_repl.wasm
goto end

:end
