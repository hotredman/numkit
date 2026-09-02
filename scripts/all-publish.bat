@echo off
setlocal

set SCRIPT_DIR=%~dp0

if "%~1"=="--help" goto show_help
if "%~1"=="-h" goto show_help

echo =======================================================
echo   [1/4] Publishing Source Code to GitHub
echo =======================================================
call "%SCRIPT_DIR%github-push.bat"
if errorlevel 1 (
    echo ERROR: github-push.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [2/4] Deploying Web IDE Demo to GitHub Pages
echo =======================================================
call "%SCRIPT_DIR%web-publish.bat"
if errorlevel 1 (
    echo ERROR: web-publish.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [3/4] Deploying Doxygen API Documentation
echo =======================================================
call "%SCRIPT_DIR%doxy-publish.bat"
if errorlevel 1 (
    echo ERROR: doxy-publish.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [4/4] Deploying Defect & Parity Catalog
echo =======================================================
call "%SCRIPT_DIR%bugs-publish.bat"
if errorlevel 1 (
    echo ERROR: bugs-publish.bat failed!
    exit /b 1
)

echo.
echo =======================================================
echo   All published successfully to GitHub!
echo   Code: https://github.com/hotredman/numkit
echo   Demo: https://hotredman.github.io/numkit-demo/
echo   Docs: https://hotredman.github.io/numkit-doxy/
echo   Bugs: https://hotredman.github.io/numkit-bugs/
echo =======================================================
exit /b 0

:show_help
echo Usage: %~nx0
echo.
echo Publishes source code (github.com/hotredman/numkit),
echo Web IDE demo (hotredman.github.io/numkit-demo),
echo Doxygen C++ API docs (hotredman.github.io/numkit-doxy),
echo and Defect & Parity Catalog (hotredman.github.io/numkit-bugs).
exit /b 0
