@echo off
setlocal

set PROJECT_DIR=%~dp0
set IDE_DIR=%PROJECT_DIR%ide
set WASM_DIST=%PROJECT_DIR%build-wasm\wasm\dist
set PAGES_DIR=%PROJECT_DIR%docs
set EMSDK=C:\Users\User\Repo\emsdk
set EMCC_DIR=%EMSDK%\upstream\emscripten

echo === mIDE Deploy to GitHub Pages ===
echo.

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo Node.js not found. Install from https://nodejs.org/
    exit /b 1
)

:: Build WASM if emsdk available and not yet built
if exist "%EMCC_DIR%\emcc.bat" (
    if not exist "%WASM_DIST%\numkit_mide.wasm" (
        echo Building WASM...
        call "%PROJECT_DIR%build.bat" --wasm
        if errorlevel 1 exit /b 1
    )
    echo Copying WASM files into ide\public\...
    copy /y "%WASM_DIST%\numkit_mide.js"   "%IDE_DIR%\public\" >nul
    copy /y "%WASM_DIST%\numkit_mide.wasm" "%IDE_DIR%\public\" >nul
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

:: Copy to docs/ for GitHub Pages
if exist "%PAGES_DIR%" rmdir /s /q "%PAGES_DIR%"
mkdir "%PAGES_DIR%"
xcopy /e /i /q "%IDE_DIR%\dist\*" "%PAGES_DIR%\" >nul
echo.> "%PAGES_DIR%\.nojekyll"

echo.
echo === Deploy complete! Files in docs/ ===
echo.
echo   git add docs/
echo   git commit -m "Deploy to GitHub Pages"
echo   git push
