@echo off
setlocal

set SCRIPT_DIR=%~dp0

if "%~1"=="--help" goto show_help
if "%~1"=="-h" goto show_help

echo =======================================================
echo   [1/3] Publishing Source Code to GitHub
echo =======================================================
call "%SCRIPT_DIR%publish-code.bat"
if errorlevel 1 (
    echo ERROR: publish-code.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [2/3] Deploying Web IDE Bundle to GitHub Pages
echo =======================================================
call "%SCRIPT_DIR%web-publish.bat" --push
if errorlevel 1 (
    echo ERROR: web-publish.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [3/3] Deploying Doxygen API Documentation
echo =======================================================
call "%SCRIPT_DIR%doxy-publish.bat" --push
if errorlevel 1 (
    echo ERROR: doxy-publish.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   All published successfully to GitHub!
echo   Code: https://github.com/hotredman/numkit
echo   Demo: https://hotredman.github.io/numkit-demo/
echo   Docs: https://hotredman.github.io/numkit-doxy/
echo =======================================================
exit /b 0

:show_help
echo Usage: %~nx0
echo.
echo Publishes both the source code (to github.com/hotredman/numkit)
echo and the Web IDE static distribution (to hotredman.github.io/numkit-demo).
exit /b 0
