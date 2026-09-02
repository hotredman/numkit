@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set DEPLOY_DIR=%PROJECT_DIR%deploy

:: Determine default pages / demo directory
set PAGES_DIR=%NUMKIT_PAGES_DIR%
if "%PAGES_DIR%"=="" (
    if exist "%PROJECT_DIR%..\..\hotredman\numkit-demo\.git" set "PAGES_DIR=%PROJECT_DIR%..\..\hotredman\numkit-demo"
    if not defined PAGES_DIR if exist "%PROJECT_DIR%..\numkit-demo\.git" set "PAGES_DIR=%PROJECT_DIR%..\numkit-demo"
    if not defined PAGES_DIR if exist "C:\Users\User\Projects\hotredman\numkit-demo\.git" set "PAGES_DIR=C:\Users\User\Projects\hotredman\numkit-demo"
    if not defined PAGES_DIR if exist "C:\Users\User\Projects\megahard\numkit-demo\.git" set "PAGES_DIR=C:\Users\User\Projects\megahard\numkit-demo"
    if not defined PAGES_DIR if exist "%PROJECT_DIR%..\numkit-pages\.git" set "PAGES_DIR=%PROJECT_DIR%..\numkit-pages"
    if not defined PAGES_DIR if exist "%PROJECT_DIR%..\numkit-web\.git" set "PAGES_DIR=%PROJECT_DIR%..\numkit-web"
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

echo === NumKit Web IDE Demo - Deploy Clean Mirror to GitHub Pages ===
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
cd /d "%PROJECT_DIR%"
for /f %%i in ('git rev-parse --short HEAD 2^>nul') do set SRC_REV=%%i
if "%SRC_REV%"=="" set SRC_REV=manual

echo.
echo Syncing deploy artifacts to "%PAGES_DIR%"...

:: Clean existing files in target (except .git)
cd /d "%PAGES_DIR%"
for /f "delims=" %%F in ('dir /b /a-d 2^>nul') do (
    if not "%%F"==".git" del /f /q "%%F" 2>nul
)
for /f "delims=" %%D in ('dir /b /ad 2^>nul') do (
    if not "%%D"==".git" rd /s /q "%%D" 2>nul
)

:: Copy fresh files from deploy/ into target repo
xcopy /e /i /y /q "%DEPLOY_DIR%\*" "%PAGES_DIR%\" >nul

:: Ensure .nojekyll exists
if not exist "%PAGES_DIR%\.nojekyll" (
    type nul > "%PAGES_DIR%\.nojekyll"
)

echo Files synchronized.

:: 3. Clean single-commit history (Orphan Branch)
echo.
echo Creating clean 1-commit state in Demo repository...
git checkout --orphan temp_deploy >nul 2>&1
git add -A
git commit -m "deploy(demo): NumKit Web IDE Demo (numkit@%SRC_REV%)" >nul 2>&1
git branch -D main >nul 2>&1
git branch -m main >nul 2>&1

if "%DO_PUSH%"=="1" (
    echo.
    echo Force-pushing single clean commit to GitHub origin main...
    git push -f origin main
    if errorlevel 1 (
        echo ERROR: git push failed!
        exit /b 1
    )
    echo.
    echo Successfully published clean 1-commit Web IDE Demo to GitHub Pages!
    goto done
) else (
    echo.
    echo Clean commit created locally (push skipped due to --no-push).
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
echo   --push           Force-push 1 clean commit to remote repository (default)
echo   --no-push        Create 1 clean commit locally without pushing
echo   --skip-build     Skip re-running web-build.bat
echo   -h, --help       Show this help message
exit /b 0
