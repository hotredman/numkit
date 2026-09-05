@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build\wasm\release\wasm\dist

where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    if exist "%DESKTOP_DIR%\dist" (
        copy /y "%WASM_DIST%\numkit_ide.js"   "%DESKTOP_DIR%\dist\" >nul
        copy /y "%WASM_DIST%\numkit_ide.wasm" "%DESKTOP_DIR%\dist\" >nul
    )
    echo WASM engine found
) else (
    echo WASM not built — fallback mode
)

set NATIVE_REPL=%PROJECT_DIR%build\windows\release\apps\numkit\Release\numkit_repl.exe
if not exist "%NATIVE_REPL%" set NATIVE_REPL=%PROJECT_DIR%build\windows\release\apps\numkit\numkit_repl.exe

if exist "%NATIVE_REPL%" (
    if exist "%PROJECT_DIR%deploy\desktop" (
        copy /y "%NATIVE_REPL%" "%PROJECT_DIR%deploy\desktop\" >nul 2>&1
    )
)

if not exist "%IDE_DIR%\node_modules" (
    echo Installing IDE dependencies...
    cd /d "%IDE_DIR%"
    call npm install
)

if not exist "%DESKTOP_DIR%\node_modules" (
    echo Installing desktop dependencies...
    cd /d "%DESKTOP_DIR%"
    call npm install
)

echo.
echo Launching Electron desktop dev shell...
echo.

cd /d "%DESKTOP_DIR%"
call npx electron .
