@echo off
setlocal

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Copy WASM artifacts if available (reused as-is; run-desktop never rebuilds
:: the engine — use build-desktop.bat for that).
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

:: ── Rebuild the renderer bundle so we ALWAYS launch the current code ──
:: Electron runs in production mode whenever desktop\dist\index.html exists
:: (see main.js IS_PROD) and loads that STATIC bundle. Without this step
:: run-desktop would relaunch whatever was last built and silently ignore
:: every code change since — the classic "it's running an old build" trap.
:: Only the JS/CSS bundle is rebuilt here (~10 s); WASM is reused from the
:: copy above. For a full WASM rebuild + packaged .exe, use build-desktop.bat.
echo.
echo Building current renderer bundle...
cd /d "%IDE_DIR%"
call npx vite build --base ./
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)
if exist "%DESKTOP_DIR%\dist" rmdir /s /q "%DESKTOP_DIR%\dist"
xcopy /e /i /q "%IDE_DIR%\dist" "%DESKTOP_DIR%\dist" >nul
echo Renderer bundle ready.

:: Install Electron if needed
if not exist "%DESKTOP_DIR%\node_modules" (
    echo Installing Electron...
    cd /d "%DESKTOP_DIR%"
    call npm install
)

echo.
echo Starting Numkit IDE...
echo.

cd /d "%DESKTOP_DIR%"
node_modules\electron\dist\electron.exe .
