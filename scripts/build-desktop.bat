@echo off
setlocal

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set DESKTOP_DIR=%IDE_DIR%\desktop
:: cmake's `browser` preset writes to ${sourceDir}/build/browser/ (since the
:: May 7 chore commit 50cc70df that consolidated all build-<preset>/ dirs
:: under a single build/<preset>/ tree). build-web.sh / build-web.bat already track
:: this path; this script previously had the legacy dashed name and silently
:: copied the May-6 stale WASM into ide/public/ on every run.
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist

:: Flags (order-independent, up to 2):
::   --skip-wasm   reuse the existing WASM in build\browser\wasm\dist
::                 (faster iteration when only IDE / JS code changed).
::                 Default: rebuild.
::   --no-package  skip the electron-builder portable-exe packaging
::                 (step 5). run-desktop.bat launches Electron directly from
::                 desktop\dist, so the packaged .exe is NOT needed for
::                 dev iteration — this skips the slow compression step.
set SKIP_WASM=0
set NO_PACKAGE=0
set SKIP_NATIVE=0
if "%1"=="--skip-wasm"   set SKIP_WASM=1
if "%2"=="--skip-wasm"   set SKIP_WASM=1
if "%3"=="--skip-wasm"   set SKIP_WASM=1
if "%1"=="--no-package"  set NO_PACKAGE=1
if "%2"=="--no-package"  set NO_PACKAGE=1
if "%3"=="--no-package"  set NO_PACKAGE=1
if "%1"=="--skip-native" set SKIP_NATIVE=1
if "%2"=="--skip-native" set SKIP_NATIVE=1
if "%3"=="--skip-native" set SKIP_NATIVE=1

echo === Numkit IDE — Desktop Build ===
echo.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: ── Step 1: rebuild WASM (calls build-engine.bat --wasm) ───────────────────
:: Engine sources change far more often than IDE shell code, so the safe
:: default is to rebuild every desktop run. Pass --skip-wasm to reuse a
:: prior build for fast IDE-only iteration.
::
:: Parallelism: build-engine.bat --wasm runs `cmake --build --preset=browser`,
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
    call "%~dp0build-engine.bat" --wasm
    if errorlevel 1 (
        echo WASM build failed!
        exit /b 1
    )
)

:: ── Step 1b: build native C++ executables (desktop-fast preset) ─────────
:: Produces numkit_repl.exe and numkit_codegen.exe in build\desktop-fast\.
:: Pass --skip-native to reuse already-built binaries (e.g. only IDE changed).
if "%SKIP_NATIVE%"=="1" (
    echo [1b/7] Skipping native C++ build (--skip-native)
) else (
    echo [1b/7] Building native executables (desktop-fast preset^)...
    cd /d "%PROJECT_DIR%"
    cmake --build --preset=desktop-fast --config Release
    if errorlevel 1 (
        echo Native C++ build failed!
        exit /b 1
    )
    echo [1b/7] numkit_repl.exe + numkit_codegen.exe ready
)

:: ── Step 2: copy WASM artifacts into ide\public\ ────────────────────
if exist "%WASM_DIST%\numkit_ide.wasm" (
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
    echo [2/7] WASM engine copied to ide\public\
) else (
    echo [2/7] WARNING: WASM not found at %WASM_DIST% — app will run in demo mode
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
    echo [3/7] Installing IDE dependencies ^(package.json newer than node_modules or fresh checkout^)...
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
:: --base ./ : the desktop shell loads dist\index.html over file://, which only
:: resolves RELATIVE asset/fetch paths. vite.config.js already defaults to
:: base './', so this is an explicit guarantee for the packaged desktop build.
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
    echo [5/7] Installing desktop dependencies ^(package.json newer than node_modules or fresh checkout^)...
    call npm install
    if errorlevel 1 exit /b 1
)

:: --no-package: desktop\dist is ready and Electron is installed, so
:: run-desktop.bat can launch directly. Skip the slow portable-exe packaging.
if "%NO_PACKAGE%"=="1" (
    echo [5/7] Skipping portable-exe packaging ^(--no-package^)
    echo.
    echo === Done ^(dev build^) ===
    echo Static files ready at %DESKTOP_DIR%\dist
    echo Launch with: run-desktop.bat
    goto :eof
)

echo [5/7] Packaging exe...
call npx electron-builder --win portable
if errorlevel 1 (
    echo electron-builder failed!
    exit /b 1
)

:: ── Step 6: assemble the deploy\desktop\ bundle ──────────────────────
:: Creates (or refreshes) a single clean folder containing exactly the
:: three executables a user needs to run Numkit on a fresh machine:
::
::   deploy\desktop\
::     numkit_ide.exe                 (Electron portable, from desktop\release\)
::     numkit_repl.exe                (interpreter, from build\desktop-fast\)
::     numkit_codegen.exe             (code generator, from build\desktop-fast\)
::
:: When the IDE starts and Settings paths are empty, main.js looks for
:: numkit.exe and numkit_codegen.exe next to its own .exe — so placing
:: all three in the same folder gives zero-config out-of-the-box operation.
set DEPLOY_DIR=%PROJECT_DIR%deploy\desktop
set CPP_RELEASE=%PROJECT_DIR%build\desktop-fast\apps

:: Wipe and recreate
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

:: Copy the IDE exe (electron-builder names it after productName + version)
for %%f in ("%DESKTOP_DIR%\release\*.exe") do (
    copy /y "%%f" "%DEPLOY_DIR%\" >nul
    echo [6/7] Copied IDE: %%~nxf
)

:: Copy numkit_repl.exe (interpreter)
set NUMKIT_EXE=%CPP_RELEASE%\numkit\Release\numkit_repl.exe
if exist "%NUMKIT_EXE%" (
    copy /y "%NUMKIT_EXE%" "%DEPLOY_DIR%\" >nul
    echo [6/7] Copied interpreter: numkit_repl.exe
) else (
    echo [6/7] WARNING: numkit_repl.exe not found at %NUMKIT_EXE%
    echo         Run without --skip-native to rebuild C++ first.
)

:: Copy numkit_codegen.exe (code generator)
set CODEGEN_EXE=%CPP_RELEASE%\numkit_codegen\Release\numkit_codegen.exe
if exist "%CODEGEN_EXE%" (
    copy /y "%CODEGEN_EXE%" "%DEPLOY_DIR%\" >nul
    echo [6/7] Copied code generator: numkit_codegen.exe
) else (
    echo [6/7] WARNING: numkit_codegen.exe not found at %CODEGEN_EXE%
    echo         Run without --skip-native to rebuild C++ first.
)

echo.
echo === Done! ===
echo Packaged exe : %DESKTOP_DIR%\release\
echo Deploy bundle: %DEPLOY_DIR%\
echo.
echo Contents of deploy\desktop\:
dir /b "%DEPLOY_DIR%"

