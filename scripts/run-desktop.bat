@echo off
setlocal

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist

:: run-desktop ONLY runs — it launches the app against the CURRENT source via
:: the Vite dev server (NUMKIT_DESKTOP_DEV below), never a prebuilt bundle. So
:: code changes show up on every launch with no build step. To produce a
:: static/packaged build, use build-desktop.bat.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Copy WASM artifacts if available (reused as-is; the engine is built by
:: build-engine.bat / build-desktop.bat, never here).
if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
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
echo Starting Numkit IDE (live source via Vite dev server)...
echo.

:: Force live/dev mode so Electron loads the Vite dev server (current source)
:: instead of a stale prebuilt desktop\dist. Electron manages the Vite process
:: and kills it on exit (main.js window-all-closed).
set NUMKIT_DESKTOP_DEV=1
cd /d "%DESKTOP_DIR%"
node_modules\electron\dist\electron.exe .
