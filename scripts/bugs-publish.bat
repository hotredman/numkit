@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set BUGS_HTML_DIR=%PROJECT_DIR%build\bugs

:: Determine default bugs repository directory
set BUGS_REPO_DIR=%NUMKIT_BUGS_DIR%
if "%BUGS_REPO_DIR%"=="" (
    if exist "%PROJECT_DIR%..\..\hotredman\numkit-bugs\.git" set "BUGS_REPO_DIR=%PROJECT_DIR%..\..\hotredman\numkit-bugs"
    if not defined BUGS_REPO_DIR if exist "%PROJECT_DIR%..\numkit-bugs\.git" set "BUGS_REPO_DIR=%PROJECT_DIR%..\numkit-bugs"
    if not defined BUGS_REPO_DIR if exist "C:\Users\User\Projects\hotredman\numkit-bugs\.git" set "BUGS_REPO_DIR=C:\Users\User\Projects\hotredman\numkit-bugs"
    if not defined BUGS_REPO_DIR if exist "C:\Users\User\Projects\megahard\numkit-bugs\.git" set "BUGS_REPO_DIR=C:\Users\User\Projects\megahard\numkit-bugs"
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
    set BUGS_REPO_DIR=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help_ok
if /i "%~1"=="-h" goto show_help_ok

set BUGS_REPO_DIR=%~1
shift
goto parse_args
:args_done

if "%BUGS_REPO_DIR%"=="" (
    echo ERROR: Destination numkit-bugs directory is not found.
    echo.
    goto show_help_err
)

if not exist "%BUGS_REPO_DIR%\.git" (
    echo ERROR: Destination directory is not a Git repository: "%BUGS_REPO_DIR%"
    echo.
    goto show_help_err
)

echo === NumKit Bugs ^& Parity - Deploy to GitHub Pages Repository ===
echo Source: %PROJECT_DIR%
echo Target: %BUGS_REPO_DIR%
echo.

:: 1. Run site generator if needed
if "%SKIP_BUILD%"=="0" (
    echo Generating NumKit Bugs ^& Parity site...
    python "%PROJECT_DIR%tools\build_bugs_site.py" --output "%BUGS_HTML_DIR%"
    if errorlevel 1 (
        echo ERROR: build_bugs_site.py failed!
        exit /b 1
    )
)

if not exist "%BUGS_HTML_DIR%\index.html" (
    echo ERROR: build\bugs\index.html not found!
    exit /b 1
)

:: 2. Get source git commit hash
set SRC_REV=
cd /d "%PROJECT_DIR%"
for /f %%i in ('git rev-parse --short HEAD 2^>nul') do set SRC_REV=%%i
if "%SRC_REV%"=="" set SRC_REV=manual

echo.
echo Syncing bugs catalog files to "%BUGS_REPO_DIR%"...

:: Copy all files from build/bugs/ into target repo
xcopy /e /i /y /q "%BUGS_HTML_DIR%\*" "%BUGS_REPO_DIR%\" >nul

:: Ensure .nojekyll exists
if not exist "%BUGS_REPO_DIR%\.nojekyll" (
    type nul > "%BUGS_REPO_DIR%\.nojekyll"
)

echo Files synchronized.

:: 3. Check git status in target repo
cd /d "%BUGS_REPO_DIR%"
git status --porcelain > "%TEMP%\bugs_status.txt"

for %%A in ("%TEMP%\bugs_status.txt") do if %%~zA==0 (
    echo.
    echo No changes detected in target repository. Target is already up to date.
    del "%TEMP%\bugs_status.txt" 2>nul
    goto done
)
del "%TEMP%\bugs_status.txt" 2>nul

echo.
echo Committing changes in Bugs repository...
git add -A
git commit -m "docs(bugs): update parity and bug catalog (numkit@%SRC_REV%)"

if "%DO_PUSH%"=="1" (
    echo.
    echo Pushing to GitHub origin main...
    git push origin main
    if errorlevel 1 (
        echo ERROR: git push failed!
        exit /b 1
    )
    echo Successfully deployed and pushed Bugs catalog to GitHub Pages!
    goto done
)

:done
cd /d "%PROJECT_DIR%"
exit /b 0

:show_help_ok
call :show_help
exit /b 0

:show_help_err
call :show_help
exit /b 1

:show_help
echo Usage: %~nx0 [options] [^<destination-dir^>]
echo.
echo Options:
echo   --dest ^<dir^>     Explicit destination repository path
echo   --push           Push committed changes to remote repository (default)
echo   --no-push        Commit changes locally without pushing
echo   --skip-build     Skip running build_bugs_site.py
echo   -h, --help       Show this help message
exit /b 0
