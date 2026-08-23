@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\

python "%SCRIPT_DIR%publish_doxy.py" %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Doxygen publishing failed with error code %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
