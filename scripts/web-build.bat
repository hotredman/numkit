@echo off
setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist
set DEPLOY_DIR=%PROJECT_DIR%deploy
if not defined EMSDK (
    if exist "%USERPROFILE%\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMSDK=%USERPROFILE%\Repo\emsdk"
    if exist "%USERPROFILE%\emsdk\upstream\emscripten\emcc.bat" set "EMSDK=%USERPROFILE%\emsdk"
    if exist "C:\Users\User\Repo\emsdk\upstream\emscripten\emcc.bat" set "EMSDK=C:\Users\User\Repo\emsdk"
)
if defined EMSDK (
    set "EMCC_DIR=!EMSDK!\upstream\emscripten"
)

echo === Numkit IDE Static Build -- output to deploy\ ===
echo.

set SKIP_WASM=0
if "%1"=="--skip-wasm" set SKIP_WASM=1
if "%2"=="--skip-wasm" set SKIP_WASM=1

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

set HAS_EMSDK=0
if exist "!EMCC_DIR!\emcc.bat" set HAS_EMSDK=1

if "!HAS_EMSDK!"=="0" (
    echo emsdk not found -- building without WASM (fallback mode only)
    goto after_wasm
)

if "!SKIP_WASM!"=="1" (
    echo [WASM] Skipping rebuild (--skip-wasm)
) else (
    echo Building WASM...
    call "%~dp0engine-build.bat" --wasm
    if errorlevel 1 exit /b 1
)

echo Copying WASM files into ide\public\...
copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul

:after_wasm

:: Generate examples manifest
if exist "%IDE_DIR%\scripts\generate-manifest.js" (
    echo Generating examples manifest...
    node "%IDE_DIR%\scripts\generate-manifest.js"
)

:: Install deps if needed
if not exist "%IDE_DIR%\node_modules" (
    echo Installing dependencies...
    cd /d "%IDE_DIR%"
    call npm install
)

:: Build Vite production bundle
echo Building Vite production bundle...
cd /d "%IDE_DIR%"
call npx vite build
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

:: Copy the built site into deploy\ (local output dir; gitignored)
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"
xcopy /e /i /q "%IDE_DIR%\dist\*" "%DEPLOY_DIR%\" >nul
echo.> "%DEPLOY_DIR%\.nojekyll"

echo.
echo === Build complete! Static IDE site in deploy\ ===
echo.
echo deploy\ is gitignored. Serve it from any static host -- the base is
echo relative, so it works at the web root or a sub-path. Preview locally
echo with "npm run preview" from the ide\ folder.
