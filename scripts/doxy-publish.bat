@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set DOCS_HTML_DIR=%PROJECT_DIR%build\docs\html

:: Determine default doxygen repository directory
set DOXY_DIR=%NUMKIT_DOXY_DIR%
if "%DOXY_DIR%"=="" (
    if exist "%PROJECT_DIR%..\..\hotredman\numkit-doxy\.git" set "DOXY_DIR=%PROJECT_DIR%..\..\hotredman\numkit-doxy"
    if not defined DOXY_DIR if exist "%PROJECT_DIR%..\numkit-doxy\.git" set "DOXY_DIR=%PROJECT_DIR%..\numkit-doxy"
    if not defined DOXY_DIR if exist "C:\Users\User\Projects\hotredman\numkit-doxy\.git" set "DOXY_DIR=C:\Users\User\Projects\hotredman\numkit-doxy"
)

set DO_PUSH=1
set SKIP_BUILD=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--push" (
    set DO_PUSH=1
    shift
    goto parse_args
)
if /i "%~1"=="--no-push" (
    set DO_PUSH=0
    shift
    goto parse_args
)
if /i "%~1"=="--skip-build" (
    set SKIP_BUILD=1
    shift
    goto parse_args
)
if /i "%~1"=="--dest" (
    set DOXY_DIR=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help_ok
if /i "%~1"=="-h" goto show_help_ok

set DOXY_DIR=%~1
shift
goto parse_args
:args_done

if "%DOXY_DIR%"=="" (
    echo ERROR: Doxygen destination directory is not specified.
    echo.
    goto show_help_err
)

if not exist "%DOXY_DIR%\.git" (
    echo ERROR: Destination directory is not a Git repository: "%DOXY_DIR%"
    echo.
    goto show_help_err
)

echo === Numkit Doxygen - Deploy to GitHub Pages Repository ===
echo Source: %PROJECT_DIR%
echo Target: %DOXY_DIR%
echo.

:: 1. Run Doxygen if needed
if "%SKIP_BUILD%"=="0" (
    echo Generating Doxygen documentation...
    cd /d "%PROJECT_DIR%"
    doxygen Doxyfile
    if errorlevel 1 (
        echo ERROR: doxygen failed!
        exit /b 1
    )
)

if not exist "%DOCS_HTML_DIR%\index.html" (
    echo ERROR: build\docs\html\index.html not found after Doxygen build!
    exit /b 1
)

:: 2. Get source git commit hash
set SRC_REV=
cd /d "%PROJECT_DIR%"
for /f %%i in ('git rev-parse --short HEAD 2^>nul') do set SRC_REV=%%i
if "%SRC_REV%"=="" set SRC_REV=manual

echo.
echo Syncing documentation files to "%DOXY_DIR%"...

:: Copy all files from build/docs/html/ into target repo
xcopy /e /i /y /q "%DOCS_HTML_DIR%\*" "%DOXY_DIR%\" >nul

:: Ensure .nojekyll exists
if not exist "%DOXY_DIR%\.nojekyll" (
    type nul > "%DOXY_DIR%\.nojekyll"
)

echo Files synchronized.

:: 3. Check git status in target repo
cd /d "%DOXY_DIR%"
git status --porcelain > "%TEMP%\doxy_status.txt"

for %%A in ("%TEMP%\doxy_status.txt") do if %%~zA==0 (
    echo.
    echo No changes detected in target repository. Target is already up to date.
    del "%TEMP%\doxy_status.txt" 2>nul
    goto done
)
del "%TEMP%\doxy_status.txt" 2>nul

echo.
echo Committing changes in Doxygen repository...
git add -A
git commit -m "docs: update Doxygen API documentation (numkit@%SRC_REV%)"

if "%DO_PUSH%"=="1" (
    echo.
    echo Pushing to GitHub origin main...
    git push origin main
    if errorlevel 1 (
        echo ERROR: git push failed!
        exit /b 1
    )
    echo Successfully deployed and pushed Doxygen docs to GitHub Pages!
    goto done
)

echo.
echo Changes committed locally in: %DOXY_DIR%
echo To push to GitHub, run:
echo   cd /d "%DOXY_DIR%"
echo   git push origin main

:done
cd /d "%PROJECT_DIR%"
echo.
echo === Done ===
exit /b 0

:show_help_ok
echo Usage: %~nx0 [--push ^| --no-push] [--skip-build] [--dest ^<path^>] [^<path^>]
echo.
echo Generates Doxygen API documentation and synchronizes it into a GitHub Pages repository.
echo.
echo Options:
echo   --push        Automatically push commit to origin main (default: on).
echo   --no-push     Commit locally without pushing to remote.
echo   --skip-build  Skip re-running doxygen if build\docs\html\ is already fresh.
echo   --dest ^<path^> Destination directory (or set NUMKIT_DOXY_DIR environment variable).
echo   -h, --help    Show this help message.
exit /b 0

:show_help_err
echo Usage: %~nx0 [--push ^| --no-push] [--skip-build] [--dest ^<path^>] [^<path^>]
echo.
echo Generates Doxygen API documentation and synchronizes it into a GitHub Pages repository.
echo.
echo Options:
echo   --push        Automatically push commit to origin main (default: on).
echo   --no-push     Commit locally without pushing to remote.
echo   --skip-build  Skip re-running doxygen if build\docs\html\ is already fresh.
echo   --dest ^<path^> Destination directory (or set NUMKIT_DOXY_DIR environment variable).
echo   -h, --help    Show this help message.
exit /b 1
