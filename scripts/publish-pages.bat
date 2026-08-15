@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set DEPLOY_DIR=%PROJECT_DIR%deploy

:: Determine default pages directory
set PAGES_DIR=%NUMKIT_PAGES_DIR%
if "%PAGES_DIR%"=="" (
    if exist "%PROJECT_DIR%..\numkit-pages\.git" set "PAGES_DIR=%PROJECT_DIR%..\numkit-pages"
    if not defined PAGES_DIR if exist "%PROJECT_DIR%..\..\czssgkavo\numkit\.git" set "PAGES_DIR=%PROJECT_DIR%..\..\czssgkavo\numkit"
    if not defined PAGES_DIR if exist "%PROJECT_DIR%..\numkit-web\.git" set "PAGES_DIR=%PROJECT_DIR%..\numkit-web"
)

set DO_PUSH=0
set SKIP_BUILD=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--push" (
    set DO_PUSH=1
    shift
    goto parse_args
)
if /i "%~1"=="--skip-build" (
    set SKIP_BUILD=1
    shift
    goto parse_args
)
if /i "%~1"=="--dest" (
    set PAGES_DIR=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help_ok
if /i "%~1"=="-h" goto show_help_ok

set PAGES_DIR=%~1
shift
goto parse_args
:args_done

if "%PAGES_DIR%"=="" (
    echo ERROR: GitHub Pages destination directory is not specified.
    echo.
    goto show_help_err
)

if not exist "%PAGES_DIR%\.git" (
    echo ERROR: Destination directory is not a Git repository: "%PAGES_DIR%"
    echo.
    goto show_help_err
)

echo === Numkit Web IDE - Deploy to GitHub Pages Repository ===
echo Source: %PROJECT_DIR%
echo Target: %PAGES_DIR%
echo.

:: 1. Build web bundle if needed
if "%SKIP_BUILD%"=="0" (
    echo Building latest Web IDE static bundle...
    call "%SCRIPT_DIR%web-build.bat"
    if errorlevel 1 (
        echo ERROR: web-build.bat failed!
        exit /b 1
    )
)

if not exist "%DEPLOY_DIR%\index.html" (
    echo ERROR: deploy\index.html not found after build!
    exit /b 1
)

:: 2. Get source git commit hash
set SRC_REV=
for /f %%i in ('git rev-parse --short HEAD 2^>nul') do set SRC_REV=%%i
if "%SRC_REV%"=="" set SRC_REV=manual

echo.
echo Syncing deploy artifacts to "%PAGES_DIR%"...

:: Clean stale assets and examples in target
if exist "%PAGES_DIR%\assets" rmdir /s /q "%PAGES_DIR%\assets"
if exist "%PAGES_DIR%\examples" rmdir /s /q "%PAGES_DIR%\examples"

:: Copy all files from deploy/ into target repo
xcopy /e /i /y /q "%DEPLOY_DIR%\*" "%PAGES_DIR%\" >nul

:: Ensure .nojekyll exists
if not exist "%PAGES_DIR%\.nojekyll" (
    type nul > "%PAGES_DIR%\.nojekyll"
)

echo Files synchronized.

:: 3. Check git status in target repo
cd /d "%PAGES_DIR%"
git status --porcelain > "%TEMP%\pages_status.txt"

for %%A in ("%TEMP%\pages_status.txt") do if %%~zA==0 (
    echo.
    echo No changes detected in target repository. Target is already up to date.
    del "%TEMP%\pages_status.txt" 2>nul
    goto done
)
del "%TEMP%\pages_status.txt" 2>nul

echo.
echo Committing changes in Pages repository...
git add -A
git commit -m "Update Web IDE build (numkit@%SRC_REV%)"

if "%DO_PUSH%"=="1" (
    echo.
    echo Pushing to GitHub origin main...
    git push origin main
    if errorlevel 1 (
        echo ERROR: git push failed!
        exit /b 1
    )
    echo Successfully deployed and pushed to GitHub Pages!
    goto done
)

echo.
echo Changes committed locally in: %PAGES_DIR%
echo To push to GitHub, run:
echo   cd /d "%PAGES_DIR%"
echo   git push origin main
echo Or pass --push next time:
echo   scripts\publish-pages.bat --push

:done
cd /d "%PROJECT_DIR%"
echo.
echo === Done ===
exit /b 0

:show_help_ok
echo Usage: %~nx0 [--push] [--skip-build] [--dest ^<path^>] [^<path^>]
echo.
echo Synchronizes the static Web IDE bundle (deploy\) into a GitHub Pages repository.
echo.
echo Options:
echo   --push        Automatically push commit to origin main in the Pages repo.
echo   --skip-build  Skip re-running web-build.bat if deploy\ is already fresh.
echo   --dest ^<path^> Destination directory (or set NUMKIT_PAGES_DIR environment variable).
echo   -h, --help    Show this help message.
exit /b 0

:show_help_err
echo Usage: %~nx0 [--push] [--skip-build] [--dest ^<path^>] [^<path^>]
echo.
echo Synchronizes the static Web IDE bundle (deploy\) into a GitHub Pages repository.
echo.
echo Options:
echo   --push        Automatically push commit to origin main in the Pages repo.
echo   --skip-build  Skip re-running web-build.bat if deploy\ is already fresh.
echo   --dest ^<path^> Destination directory (or set NUMKIT_PAGES_DIR environment variable).
echo   -h, --help    Show this help message.
exit /b 1
