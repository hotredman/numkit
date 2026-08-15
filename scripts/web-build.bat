@echo off
setlocal

set PROJECT_DIR=%~dp0..\
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build\browser\wasm\dist
set DEPLOY_DIR=%PROJECT_DIR%deploy
if not defined EMSDK (
    if exist "%USERPROFILE%\Repo\emsdk" set "EMSDK=%USERPROFILE%\Repo\emsdk"
    if exist "%USERPROFILE%\emsdk" set "EMSDK=%USERPROFILE%\emsdk"
)
set EMCC_DIR=%EMSDK%\upstream\emscripten

echo === Numkit IDE Static Build -- output to deploy\ ===
echo.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Build WASM if emsdk available and not yet built
if exist "%EMCC_DIR%\emcc.bat" (
    if not exist "%WASM_DIST%\numkit_ide.wasm" (
        echo Building WASM...
        call "%~dp0engine-build.bat" --wasm
        if errorlevel 1 exit /b 1
    )
    echo Copying WASM files into ide\public\...
    copy /y "%WASM_DIST%\numkit_ide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_ide.wasm" "%IDE_DIR%\public\" >nul
) else (
    echo emsdk not found — building without WASM (fallback mode only)
)

:: Generate examples manifest
if exist "%IDE_DIR%\scripts\generate-manifest.js" (
    echo Generating examples manifest...
    node "%IDE_DIR%\scripts\generate-manifest.js"
)

:: Install deps if needed
if not exist "%IDE_DIR%\node_modules" (
    echo Installing dependencies...
    cd /d "%IDE_DIR%"
    call npm install
)

:: Build Vite production bundle
echo Building Vite production bundle...
cd /d "%IDE_DIR%"
call npx vite build
if errorlevel 1 (
    echo Vite build failed!
    exit /b 1
)

:: Copy the built site into deploy\ (local output dir; gitignored)
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"
xcopy /e /i /q "%IDE_DIR%\dist\*" "%DEPLOY_DIR%\" >nul
echo.> "%DEPLOY_DIR%\.nojekyll"

echo.
echo === Build complete! Static IDE site in deploy\ ===
echo.
echo deploy\ is gitignored. Serve it from any static host -- the base is
echo relative, so it works at the web root or a sub-path. Preview locally
echo with "npm run preview" from the ide\ folder.
