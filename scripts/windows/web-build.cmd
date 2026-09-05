@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build\wasm\release\wasm\dist
set DEPLOY_DIR=%PROJECT_DIR%deploy

if not defined EMSDK (
    if exist "C:\Users\User\Repo\emsdk" set "EMSDK=C:\Users\User\Repo\emsdk"
    if exist "%USERPROFILE%\Repo\emsdk" set "EMSDK=%USERPROFILE%\Repo\emsdk"
    if exist "%USERPROFILE%\emsdk" set "EMSDK=%USERPROFILE%\emsdk"
)
if not defined EMCC_DIR (
    if exist "C:\Users\User\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMCC_DIR=C:\Users\User\Repo\emsdk\upstream\emscripten"
    if exist "%USERPROFILE%\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMCC_DIR=%USERPROFILE%\Repo\emsdk\upstream\emscripten"
)

echo === Numkit IDE Static Build -- output to deploy\ ===
echo.

if "%1"=="--help" goto show_help
if "%1"=="-h"     goto show_help

set SKIP_WASM=0
if "%1"=="--skip-wasm" set SKIP_WASM=1
if "%2"=="--skip-wasm" set SKIP_WASM=1

where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

if not exist "%EMCC_DIR%\emcc.bat" (
    echo ERROR: emsdk not found -- refusing to silently reuse a possibly-stale WASM.
    echo        Install emsdk or pass --skip-wasm to reuse explicitly.
    exit /b 1
)

if "%SKIP_WASM%"=="1" (
    echo [WASM] Skipping rebuild ^(--skip-wasm^)
) else (
    echo Building WASM ^(wasm-release^)...
    call "%SCRIPT_DIR%engine-build.cmd" --wasm
    if errorlevel 1 exit /b 1
)

echo Copying WASM files into ide\public\...
copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul

if exist "%IDE_DIR%\scripts\generate-manifest.js" (
    echo Generating examples manifest...
    node "%IDE_DIR%\scripts\generate-manifest.js"
)

if not exist "%IDE_DIR%\node_modules" (
    echo Installing dependencies...
    cd /d "%IDE_DIR%"
    call npm install
)

echo Building Vite production bundle...
cd /d "%IDE_DIR%"
call npx vite build
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"
xcopy /e /i /q "%IDE_DIR%\dist\*" "%DEPLOY_DIR%\" >nul
echo.> "%DEPLOY_DIR%\.nojekyll"

echo.
echo === Build complete! Static IDE site in deploy\ ===
exit /b 0

:show_help
echo Usage: web-build.cmd [--skip-wasm]
echo.
echo Builds the static Web IDE distribution to deploy\web\.
echo.
echo Options:
echo   --skip-wasm  Reuse existing WASM files without rebuilding
echo   -h, --help   Show this help message
exit /b 0
