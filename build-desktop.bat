@echo off
setlocal

set PROJECT_DIR=%~dp0
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build-browser\wasm\dist

echo === mIDE — Desktop Build ===
echo.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Copy WASM artifacts
if exist "%WASM_DIST%\numkit_mide.wasm" (
    copy /y "%WASM_DIST%\numkit_mide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_mide.wasm" "%IDE_DIR%\public\" >nul
    echo [1/4] WASM engine copied
) else (
    echo [1/4] WARNING: WASM not built — app will run in demo mode
)

:: Install IDE dependencies
if not exist "%IDE_DIR%\node_modules" (
    echo [2/4] Installing IDE dependencies...
    cd /d "%IDE_DIR%"
    call npm install
) else (
    echo [2/4] IDE dependencies OK
)

:: Build Vite static files
echo [3/4] Building static files...
cd /d "%IDE_DIR%"
call npx vite build --base ./
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

:: Copy dist to desktop
if exist "%DESKTOP_DIR%\dist" rmdir /s /q "%DESKTOP_DIR%\dist"
xcopy /e /i /q "%IDE_DIR%\dist" "%DESKTOP_DIR%\dist" >nul
echo      Static files ready

:: Install desktop dependencies
cd /d "%DESKTOP_DIR%"
if not exist "node_modules" (
    call npm install
)

:: Build exe
echo [4/4] Packaging exe...
call npx electron-builder --win portable
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo === Done! ===
echo Output: %DESKTOP_DIR%\release\
dir /b "%DESKTOP_DIR%\release\*.exe" 2>nul
