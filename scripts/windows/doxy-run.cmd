@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
set DOCS_HTML_DIR=%PROJECT_DIR%build\docs\html
set PORT=8080

set SKIP_BUILD=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--skip-build" (
    set SKIP_BUILD=1
    shift
    goto parse_args
)
if /i "%~1"=="--port" (
    set PORT=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help

shift
goto parse_args
:args_done

if not exist "%DOCS_HTML_DIR%\index.html" set SKIP_BUILD=0
if "%SKIP_BUILD%"=="0" (
    echo Building Doxygen documentation...
    cd /d "%PROJECT_DIR%"
    doxygen Doxyfile
    if errorlevel 1 (
        echo ERROR: doxygen build failed!
        exit /b 1
    )
)

echo.
echo ========================================================
echo   Numkit Doxygen Documentation Local Server
echo   URL:     http://localhost:%PORT%/
echo   Serving: %DOCS_HTML_DIR%
echo ========================================================
echo Press Ctrl+C to stop the server.
echo.

start http://localhost:%PORT%/
python -m http.server %PORT% --directory "%DOCS_HTML_DIR%"
exit /b 0

:show_help
echo Usage: %~nx0 [--skip-build] [--port ^<number^>]
echo.
echo Builds (if needed) and serves Doxygen documentation locally.
exit /b 0
