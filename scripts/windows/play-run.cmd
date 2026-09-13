@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set PLAY_DIR=%PROJECT_DIR%play
set WASM_DIST=%PROJECT_DIR%build\wasm\release\wasm\dist

if not defined EMSDK (
    if exist "C:\Users\User\Repo\emsdk" set "EMSDK=C:\Users\User\Repo\emsdk"
    if exist "%USERPROFILE%\Repo\emsdk" set "EMSDK=%USERPROFILE%\Repo\emsdk"
    if exist "%USERPROFILE%\emsdk" set "EMSDK=%USERPROFILE%\emsdk"
)
if not defined EMCC_DIR (
    if exist "C:\Users\User\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMCC_DIR=C:\Users\User\Repo\emsdk\upstream\emscripten"
    if exist "%USERPROFILE%\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMCC_DIR=%USERPROFILE%\Repo\emsdk\upstream\emscripten"
)

echo === NK Play dev server ===
echo.

if "%1"=="--help" goto show_help
if "%1"=="-h"     goto show_help

set SKIP_WASM=0
if "%1"=="--skip-wasm" set SKIP_WASM=1

where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

if "%SKIP_WASM%"=="1" (
    echo [WASM] Skipping rebuild ^(--skip-wasm^)
) else (
    if not exist "%EMCC_DIR%\emcc.bat" (
        echo WARNING: emsdk not found -- reusing the last built wasm artifact.
        echo          Run scripts\windows\engine-build.cmd --wasm for a fresh build.
    ) else (
        echo Building WASM ^(wasm-release^)...
        call "%SCRIPT_DIR%engine-build.cmd" --wasm
        if errorlevel 1 exit /b 1
    )
)

if not exist "%PLAY_DIR%\node_modules" (
    echo Installing play dependencies...
    pushd "%PLAY_DIR%"
    call npm install --no-fund --no-audit
    popd
)

echo Syncing wasm glue into play\public\ ...
if not exist "%WASM_DIST%\nk_ide.js" (
    echo ERROR: %WASM_DIST%\nk_ide.js not found.
    echo        Build it first: scripts\windows\engine-build.cmd --wasm
    exit /b 1
)
node "%PLAY_DIR%\scripts\sync-wasm.mjs"
if errorlevel 1 exit /b 1

echo Generating gallery index...
node "%PLAY_DIR%\scripts\build-gallery.mjs"
if errorlevel 1 exit /b 1

echo.
echo Starting NK Play at http://localhost:5173 ...
echo (Ctrl+C to stop)
pushd "%PLAY_DIR%"
call npx vite --host
popd
goto :eof

:show_help
echo Usage: scripts\windows\play-run.cmd [--skip-wasm]
echo.
echo   Builds the wasm engine (unless --skip-wasm), syncs the glue into
echo   play\public\, regenerates the gallery index, and starts the NK
echo   Play dev server at http://localhost:5173.
goto :eof
