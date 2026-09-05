@echo off
setlocal

set SCRIPT_DIR=%~dp0

if "%~1"=="--help" goto show_help
if "%~1"=="-h" goto show_help

echo =======================================================
echo   [1/4] Publishing Source Code to GitHub
echo =======================================================
call "%SCRIPT_DIR%code-publish.cmd"
if errorlevel 1 (
    echo ERROR: code-publish.cmd failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [2/4] Deploying Web IDE Demo to GitHub Pages
echo =======================================================
call "%SCRIPT_DIR%web-publish.cmd"
if errorlevel 1 (
    echo ERROR: web-publish.cmd failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [3/4] Deploying Doxygen API Documentation
echo =======================================================
call "%SCRIPT_DIR%doxy-publish.cmd"
if errorlevel 1 (
    echo ERROR: doxy-publish.cmd failed!
    exit /b 1
)

echo.
echo =======================================================
echo   [4/4] Deploying Defect & Parity Catalog
echo =======================================================
call "%SCRIPT_DIR%bugs-publish.cmd"
if errorlevel 1 (
    echo ERROR: bugs-publish.cmd failed!
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
echo Publishes source code, Web IDE demo, Doxygen docs, and Defect catalog to GitHub.
exit /b 0
