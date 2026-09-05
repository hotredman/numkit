@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build\wasm\release\wasm\dist

where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    echo WASM engine found
) else (
    echo WASM not built — fallback mode
)

set NEED_IDE_INSTALL=0
if not exist "%IDE_DIR%\node_modules" set NEED_IDE_INSTALL=1
if "%NEED_IDE_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%IDE_DIR%\package.json').LastWriteTime -gt (Get-Item '%IDE_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_IDE_INSTALL=1
)
if "%NEED_IDE_INSTALL%"=="1" (
    echo Installing IDE dependencies...
    cd /d "%IDE_DIR%"
    call npm install
    if errorlevel 1 exit /b 1
)

echo.
echo Starting dev server at http://127.0.0.1:3000...
echo.

cd /d "%IDE_DIR%"
call npm run dev -- --host 127.0.0.1 --port 3000
