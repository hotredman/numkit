@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..\
cd /d "%PROJECT_DIR%"

set "PVS_CMD=PVS-Studio_Cmd.exe"
set "PLOG_CONV=PlogConverter.exe"
if exist "C:\Program Files (x86)\PVS-Studio\PVS-Studio_Cmd.exe" set "PVS_CMD=C:\Program Files (x86)\PVS-Studio\PVS-Studio_Cmd.exe"
if exist "C:\Program Files (x86)\PVS-Studio\PlogConverter.exe" set "PLOG_CONV=C:\Program Files (x86)\PVS-Studio\PlogConverter.exe"

set SLN_PATH=%PROJECT_DIR%build\windows\release\nk.sln
set OUT_DIR=%PROJECT_DIR%build\pvs
set PLOG_PATH=%OUT_DIR%\nk.plog
set JSON_PATH=%OUT_DIR%\nk.plog.json
set INPUT_JSON=
set DO_COMMIT=0

:parse_args
if "%~1"=="" goto after_args
if "%~1"=="-h" goto show_help
if "%~1"=="--help" goto show_help
if "%~1"=="--all" (
    set "SLN_PATH=%PROJECT_DIR%build\windows\release\nk.sln"
    shift
    goto parse_args
)
if "%~1"=="--core" (
    set "SLN_PATH=%PROJECT_DIR%build\windows\release\nk_core_obj.vcxproj"
    shift
    goto parse_args
)
if "%~1"=="--toolboxes" (
    set "SLN_PATH=%PROJECT_DIR%build\windows\release\nk_toolboxes_obj.vcxproj"
    shift
    goto parse_args
)
if "%~1"=="--bundle" (
    set "SLN_PATH=%PROJECT_DIR%build\windows\release\nk_bundle_obj.vcxproj"
    shift
    goto parse_args
)
if "%~1"=="--commit" (
    set DO_COMMIT=1
    shift
    goto parse_args
)
if "%~1"=="--input" (
    set INPUT_JSON=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--target" (
    set SLN_PATH=%~2
    shift
    shift
    goto parse_args
)
echo Unknown argument: %~1
goto show_help

:after_args
if not "%INPUT_JSON%"=="" (
    echo Using provided input report: %INPUT_JSON%
    goto process_report
)

if not exist "%PVS_CMD%" (
    where PVS-Studio_Cmd.exe >nul 2>&1
    if errorlevel 1 (
        echo ERROR: PVS-Studio_Cmd.exe not found on PATH or default install path.
        echo        Ensure PVS-Studio is installed or pass --input ^<report.json^>.
        exit /b 1
    )
)

if not exist "%SLN_PATH%" (
    echo Configuring windows-release preset...
    cmake --preset windows-release
    if errorlevel 1 (
        echo ERROR: CMake configure failed!
        exit /b 1
    )
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if exist "%PLOG_PATH%" del /f /q "%PLOG_PATH%"
if exist "%JSON_PATH%" del /f /q "%JSON_PATH%"

echo =======================================================
echo   Running PVS-Studio Analysis on %SLN_PATH%
echo =======================================================
"%PVS_CMD%" -t "%SLN_PATH%" -c Release -p x64 -C "%PROJECT_DIR%.pvsconfig" -o "%PLOG_PATH%" --securityRelatedIssues --disableLicenseExpirationCheck
set PVS_EXIT=%ERRORLEVEL%

rem PVS exit code 256 indicates warnings found in source, which is expected.
rem Mask 1, 2, 4, 8, 16, 32, 64, 128 indicate real execution errors.
set /a IS_ERROR="PVS_EXIT & 255"
if not "%IS_ERROR%"=="0" (
    echo ERROR: PVS-Studio analysis failed with exit code %PVS_EXIT%
    exit /b %PVS_EXIT%
)

if not exist "%PLOG_PATH%" (
    echo ERROR: Analysis output %PLOG_PATH% was not created!
    exit /b 1
)

echo Converting log to JSON...
"%PLOG_CONV%" -t JSON -m CWE -e "build;thirdparty;node_modules;fieldtest;.vs" -o "%OUT_DIR%" -r "%PROJECT_DIR%\" "%PLOG_PATH%"
if not exist "%JSON_PATH%" (
    echo ERROR: PlogConverter failed to generate %JSON_PATH%!
    exit /b 1
)
set INPUT_JSON=%JSON_PATH%

:process_report
echo Processing summary...
set COMMIT_FLAG=
if "%DO_COMMIT%"=="1" set COMMIT_FLAG=--commit

python tools\pvs_report_summary.py --input "%INPUT_JSON%" %COMMIT_FLAG%
exit /b %ERRORLEVEL%

:show_help
echo Usage: %~nx0 [options]
echo.
echo Runs PVS-Studio static analysis and updates bugs\pvs\ reports.
echo.
echo Options:
echo   --all            Scan full project solution (default: nk.sln)
echo   --core           Scan interpreter core only (nk_core_obj)
echo   --toolboxes      Scan numerical toolboxes only (nk_toolboxes_obj)
echo   --bundle         Scan bundle wiring and registrations (nk_bundle_obj)
echo   --target ^<path^>  Specify custom solution or vcxproj file
echo   --commit         Automatically commit the generated report to git (if working tree clean)
echo   --input ^<file^>   Process an existing PVS JSON report instead of running analyzer
echo   -h, --help       Show this help message
exit /b 0
