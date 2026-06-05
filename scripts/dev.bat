@echo off
setlocal

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Copy WASM artifacts if available
if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    echo WASM engine found
) else (
    echo WASM not built — fallback mode
)

:: Install dependencies if needed.
:: Don't just check for node_modules existence — after a git pull /
:: branch merge that adds a new dep, the folder is there but stale,
:: vite then dies with "Rollup failed to resolve import". Compare
:: package.json mtime against node_modules mtime; reinstall when
:: package.json is newer. Same logic as build-desktop.bat.
set NEED_IDE_INSTALL=0
if not exist "%IDE_DIR%\node_modules" set NEED_IDE_INSTALL=1
if "%NEED_IDE_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%IDE_DIR%\package.json').LastWriteTime -gt (Get-Item '%IDE_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_IDE_INSTALL=1
)
if "%NEED_IDE_INSTALL%"=="1" (
    echo Installing IDE dependencies ^(package.json newer than node_modules or fresh checkout^)...
    cd /d "%IDE_DIR%"
    call npm install
    if errorlevel 1 exit /b 1
)

echo.
echo Starting dev server...
echo.

:: Use `npm run dev` (not direct vite invocation) so the `predev`
:: hook fires and regenerates the examples manifest before serving.
cd /d "%IDE_DIR%"
call npm run dev -- --host 127.0.0.1 --port 3000
