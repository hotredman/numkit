@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
set WASM_DIST=%PROJECT_DIR%build\wasm\release\wasm\dist

set SKIP_WASM=0
set NO_PACKAGE=0
set SKIP_NATIVE=0
if "%1"=="--help" goto show_help
if "%1"=="-h"     goto show_help
if "%1"=="--skip-wasm"   set SKIP_WASM=1
if "%2"=="--skip-wasm"   set SKIP_WASM=1
if "%3"=="--skip-wasm"   set SKIP_WASM=1
if "%1"=="--no-package"  set NO_PACKAGE=1
if "%2"=="--no-package"  set NO_PACKAGE=1
if "%3"=="--no-package"  set NO_PACKAGE=1
if "%1"=="--skip-native" set SKIP_NATIVE=1
if "%2"=="--skip-native" set SKIP_NATIVE=1
if "%3"=="--skip-native" set SKIP_NATIVE=1

echo === Numkit IDE — Desktop Build (Windows) ===
echo.

where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

if "%SKIP_WASM%"=="1" (
    if exist "%WASM_DIST%\numkit_ide.wasm" (
        echo [1/5] Skipping WASM rebuild (--skip-wasm; reusing existing build^)
    ) else (
        echo [1/5] WARNING: --skip-wasm but no WASM at %WASM_DIST% — falling through to rebuild
        set SKIP_WASM=0
    )
)
if not "%SKIP_WASM%"=="1" (
    echo [1/5] Rebuilding WASM engine via wasm-release preset...
    call "%SCRIPT_DIR%engine-build.cmd" --wasm
    if errorlevel 1 (
        echo WASM build failed!
        exit /b 1
    )
)

if "%SKIP_NATIVE%"=="1" (
    echo [1b/7] Skipping native C++ build ^(--skip-native^)
) else (
    echo [1b/7] Building native executables (windows-release preset^)...
    cd /d "%PROJECT_DIR%"
    cmake --build --preset=windows-release --config Release
    if errorlevel 1 (
        echo Native C++ build failed!
        exit /b 1
    )
    echo [1b/7] Native C++ binaries ready
)

if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    echo [2/7] WASM engine copied to ide\public\
) else (
    echo [2/7] WARNING: WASM not found at %WASM_DIST% — app will run in demo mode
)

set NEED_IDE_INSTALL=0
if not exist "%IDE_DIR%\node_modules" set NEED_IDE_INSTALL=1
if "%NEED_IDE_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%IDE_DIR%\package.json').LastWriteTime -gt (Get-Item '%IDE_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_IDE_INSTALL=1
)
if "%NEED_IDE_INSTALL%"=="1" (
    echo [3/7] Installing IDE dependencies...
    cd /d "%IDE_DIR%"
    call npm install
    if errorlevel 1 exit /b 1
) else (
    echo [3/7] IDE dependencies OK
)

echo [4/7] Generating examples manifest and building static files...
cd /d "%IDE_DIR%"
if exist "scripts\generate-manifest.js" (
    node "scripts\generate-manifest.js"
)
call npx vite build --base ./
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

if exist "%DESKTOP_DIR%\dist" rmdir /s /q "%DESKTOP_DIR%\dist"
xcopy /e /i /q "%IDE_DIR%\dist" "%DESKTOP_DIR%\dist" >nul
echo      Static files ready at %DESKTOP_DIR%\dist

set NEED_DSK_INSTALL=0
if not exist "%DESKTOP_DIR%\node_modules" set NEED_DSK_INSTALL=1
if "%NEED_DSK_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%DESKTOP_DIR%\package.json').LastWriteTime -gt (Get-Item '%DESKTOP_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_DSK_INSTALL=1
)
cd /d "%DESKTOP_DIR%"
if "%NEED_DSK_INSTALL%"=="1" (
    echo [5/7] Installing desktop dependencies...
    call npm install
    if errorlevel 1 exit /b 1
)

if "%NO_PACKAGE%"=="1" (
    echo [5/7] Skipping portable-exe packaging ^(--no-package^)
    echo.
    echo === Done ^(dev build^) ===
    echo Static files ready at %DESKTOP_DIR%\dist
    echo Launch with: scripts\windows\desktop-run.cmd
    goto :eof
)

echo [5/7] Packaging exe...
call npx electron-builder --win portable
if errorlevel 1 (
    echo electron-builder failed!
    exit /b 1
)

set DEPLOY_DIR=%PROJECT_DIR%deploy\desktop
set CPP_RELEASE=%PROJECT_DIR%build\windows\release\apps

if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

for %%f in ("%DESKTOP_DIR%\release\*.exe") do (
    copy /y "%%f" "%DEPLOY_DIR%\" >nul
    echo [6/7] Copied IDE: %%~nxf
)

set NUMKIT_EXE=%CPP_RELEASE%\numkit\Release\numkit_repl.exe
if not exist "%NUMKIT_EXE%" set NUMKIT_EXE=%CPP_RELEASE%\numkit\numkit_repl.exe
if exist "%NUMKIT_EXE%" (
    copy /y "%NUMKIT_EXE%" "%DEPLOY_DIR%\" >nul
    echo [6/7] Copied interpreter: numkit_repl.exe
) else (
    echo [6/7] WARNING: numkit_repl.exe not found at %NUMKIT_EXE%
)

echo.
echo === Done! ===
echo Packaged exe : %DESKTOP_DIR%\release\
echo Deploy bundle: %DEPLOY_DIR%\
exit /b 0

:show_help
echo Usage: desktop-build.cmd [--skip-wasm] [--skip-native] [--no-package]
echo.
echo Builds the Numkit Desktop application using Electron and CMake.
echo.
echo Options:
echo   --skip-wasm    Do not rebuild WASM engine (reuse existing build)
echo   --skip-native  Do not rebuild native C++ binaries
echo   --no-package   Build frontend bundle only, skip electron-builder
echo   -h, --help     Show this help message
exit /b 0
