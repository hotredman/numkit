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

:: Copy WASM artifacts if available
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

:: Sync native binaries into deploy\desktop if available
if exist "%PROJECT_DIR%build\desktop-fast\apps\numkit\Release\numkit_repl.exe" (
    if exist "%PROJECT_DIR%deploy\desktop" (
        copy /y "%PROJECT_DIR%build\desktop-fast\apps\numkit\Release\numkit_repl.exe" "%PROJECT_DIR%deploy\desktop\" >nul 2>&1
    )
)
if exist "%PROJECT_DIR%build\desktop-fast\apps\numkit_codegen\Release\numkit_codegen.exe" (
    if exist "%PROJECT_DIR%deploy\desktop" (
        copy /y "%PROJECT_DIR%build\desktop-fast\apps\numkit_codegen\Release\numkit_codegen.exe" "%PROJECT_DIR%deploy\desktop\" >nul 2>&1
    )
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

:: Sync examples manifest across public/ and dist/
if exist "%IDE_DIR%\scripts\generate-manifest.js" (
    node "%IDE_DIR%\scripts\generate-manifest.js"
)

set RUN_MODE=--dev
if "%1"=="--prod" set RUN_MODE=
if "%1"=="--dist" set RUN_MODE=

echo.
echo Starting Numkit IDE...
echo.

cd /d "%DESKTOP_DIR%"
node_modules\electron\dist\electron.exe . %RUN_MODE%
