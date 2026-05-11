@echo off
setlocal

set PROJECT_DIR=%~dp0
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
:: cmake's `browser` preset writes to ${sourceDir}/build/browser/ (since the
:: May 7 chore commit 50cc70df that consolidated all build-<preset>/ dirs
:: under a single build/<preset>/ tree). deploy.sh / deploy.bat already track
:: this path; this script previously had the legacy dashed name and silently
:: copied the May-6 stale WASM into ide/public/ on every run.
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist

:: Flags:
::   --skip-wasm  reuse the existing WASM in build-browser\wasm\dist (faster
::                iteration when only IDE / JS code changed). Default: rebuild.
set SKIP_WASM=0
if "%1"=="--skip-wasm" set SKIP_WASM=1
if "%2"=="--skip-wasm" set SKIP_WASM=1

echo === Numkit IDE — Desktop Build ===
echo.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: ── Step 1: rebuild WASM (calls build.bat --wasm) ───────────────────
:: Engine sources change far more often than IDE shell code, so the safe
:: default is to rebuild every desktop run. Pass --skip-wasm to reuse a
:: prior build for fast IDE-only iteration.
::
:: Parallelism: build.bat --wasm runs `cmake --build --preset=browser`,
:: which is file-level parallel via `"jobs": 0` in CMakePresets.json
:: (browser buildPreset). Combined with `if(MSVC) /MP` in CMakeLists
:: (no-op for emcc but harmless), this saturates all cores during the
:: WASM compile. The npm install / vite / electron-builder steps below
:: have their own parallelism and run sequentially per the dependency
:: graph (vite needs the WASM artefacts; electron-builder needs vite).
if "%SKIP_WASM%"=="1" (
    if exist "%WASM_DIST%\numkit_ide.wasm" (
        echo [1/5] Skipping WASM rebuild (--skip-wasm; reusing existing build^)
    ) else (
        echo [1/5] WARNING: --skip-wasm but no WASM at %WASM_DIST% — falling through to rebuild
        set SKIP_WASM=0
    )
)
if not "%SKIP_WASM%"=="1" (
    echo [1/5] Rebuilding WASM engine -- parallel via browser preset jobs:0 ...
    call "%PROJECT_DIR%build.bat" --wasm
    if errorlevel 1 (
        echo WASM build failed!
        exit /b 1
    )
)

:: ── Step 2: copy WASM artifacts into ide\public\ ────────────────────
if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    echo [2/5] WASM engine copied to ide\public\
) else (
    echo [2/5] WARNING: WASM not found at %WASM_DIST% — app will run in demo mode
)

:: ── Step 3: install IDE deps + Vite build ───────────────────────────
:: Don't just check for node_modules existence — after a git pull /
:: branch merge that adds a new dep (e.g. `three`), the folder is
:: there but stale, vite then dies with "Rollup failed to resolve
:: import 'three'". Use PowerShell to compare package.json mtime
:: against node_modules mtime; reinstall when package.json is newer.
set NEED_IDE_INSTALL=0
if not exist "%IDE_DIR%\node_modules" set NEED_IDE_INSTALL=1
if "%NEED_IDE_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%IDE_DIR%\package.json').LastWriteTime -gt (Get-Item '%IDE_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_IDE_INSTALL=1
)
if "%NEED_IDE_INSTALL%"=="1" (
    echo [3/5] Installing IDE dependencies ^(package.json newer than node_modules or fresh checkout^)...
    cd /d "%IDE_DIR%"
    call npm install
    if errorlevel 1 exit /b 1
) else (
    echo [3/5] IDE dependencies OK
)

echo [4/5] Building static files...
cd /d "%IDE_DIR%"
call npx vite build --base ./
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

:: Copy dist to desktop
if exist "%DESKTOP_DIR%\dist" rmdir /s /q "%DESKTOP_DIR%\dist"
xcopy /e /i /q "%IDE_DIR%\dist" "%DESKTOP_DIR%\dist" >nul
echo      Static files ready at %DESKTOP_DIR%\dist

:: ── Step 5: install desktop deps + package exe ──────────────────────
:: Same staleness check as Step 3.
set NEED_DSK_INSTALL=0
if not exist "%DESKTOP_DIR%\node_modules" set NEED_DSK_INSTALL=1
if "%NEED_DSK_INSTALL%"=="0" (
    powershell -NoProfile -Command "if ((Get-Item '%DESKTOP_DIR%\package.json').LastWriteTime -gt (Get-Item '%DESKTOP_DIR%\node_modules').LastWriteTime) { exit 1 } else { exit 0 }"
    if errorlevel 1 set NEED_DSK_INSTALL=1
)
cd /d "%DESKTOP_DIR%"
if "%NEED_DSK_INSTALL%"=="1" (
    echo Installing desktop dependencies ^(package.json newer than node_modules or fresh checkout^)...
    call npm install
    if errorlevel 1 exit /b 1
)

echo [5/5] Packaging exe...
call npx electron-builder --win portable
if errorlevel 1 (
    echo electron-builder failed!
    exit /b 1
)

echo.
echo === Done! ===
echo Output: %DESKTOP_DIR%\release\
dir /b "%DESKTOP_DIR%\release\*.exe" 2>nul
