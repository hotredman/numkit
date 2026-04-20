@echo off
setlocal

set PROJECT_DIR=%~dp0
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build-browser\wasm\dist

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Copy WASM artifacts if available
if exist "%WASM_DIST%\numkit_mide.wasm" (
    copy /y "%WASM_DIST%\numkit_mide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_mide.wasm" "%IDE_DIR%\public\" >nul
    echo WASM engine found
) else (
    echo WASM not built — fallback mode
)

:: Install IDE dependencies if needed
if not exist "%IDE_DIR%\node_modules" (
    echo Installing IDE dependencies...
    cd /d "%IDE_DIR%"
    call npm install
)

:: Install Electron if needed
if not exist "%DESKTOP_DIR%\node_modules" (
    echo Installing Electron...
    cd /d "%DESKTOP_DIR%"
    call npm install
)

echo.
echo Starting mIDE...
echo.

cd /d "%DESKTOP_DIR%"
node_modules\electron\dist\electron.exe .
