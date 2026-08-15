@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\

:: Determine GitHub code remote URL
set CODE_REMOTE=%NUMKIT_GITHUB_REMOTE%
set LOCAL_MIRROR=

if exist "%PROJECT_DIR%..\..\hotredman\numkit\.git" (
    set "LOCAL_MIRROR=%PROJECT_DIR%..\..\hotredman\numkit"
) else if exist "%PROJECT_DIR%..\numkit-src\.git" (
    set "LOCAL_MIRROR=%PROJECT_DIR%..\numkit-src"
)

if "%CODE_REMOTE%"=="" (
    :: Try reading from existing github remote
    for /f "tokens=*" %%i in ('git -C "%PROJECT_DIR:~0,-1%" remote get-url github 2^>nul') do set CODE_REMOTE=%%i
)
if "%CODE_REMOTE%"=="" (
    :: Try reading from local clone origin
    if defined LOCAL_MIRROR (
        for /f "tokens=*" %%i in ('git -C "%LOCAL_MIRROR%" remote get-url origin 2^>nul') do set CODE_REMOTE=%%i
    )
)
if "%CODE_REMOTE%"=="" set "CODE_REMOTE=git@github.com:hotredman/numkit.git"

:: Parse arguments
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--remote" (
    set CODE_REMOTE=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help_ok
if /i "%~1"=="-h" goto show_help_ok

set CODE_REMOTE=%~1
shift
goto parse_args
:args_done

echo === Numkit - Publish Source Code to GitHub ===
echo Source: %PROJECT_DIR%
echo GitHub Remote: %CODE_REMOTE%
echo.

cd /d "%PROJECT_DIR%"

:: Ensure github remote is configured
git remote get-url github >nul 2>&1
if errorlevel 1 (
    echo Adding remote "github" -^> %CODE_REMOTE%
    git remote add github "%CODE_REMOTE%"
) else (
    git remote set-url github "%CODE_REMOTE%"
)

:: Push main branch and tags
echo.
echo Pushing main branch to GitHub...
git push github main
if errorlevel 1 (
    echo ERROR: Failed to push main branch to %CODE_REMOTE%!
    exit /b 1
)

echo.
echo Pushing tags to GitHub...
git push github --tags >nul 2>&1

:: Sync local mirror directory if present
if defined LOCAL_MIRROR (
    if exist "%LOCAL_MIRROR%\.git" (
        echo.
        echo Syncing local mirror clone ^(%LOCAL_MIRROR%^)...
        git -C "%LOCAL_MIRROR%" pull origin main >nul 2>&1
    )
)

echo.
echo === Source code successfully published to GitHub! ===
exit /b 0

:show_help_ok
echo Usage: %~nx0 [--remote ^<git_url^>] [^<git_url^>]
echo.
echo Pushes the main branch and tags from the primary repository to GitHub.
echo.
echo Options:
echo   --remote ^<url^>  GitHub repository URL ^(default: auto-detected or git@github.com:hotredman/numkit.git^).
echo   -h, --help      Show this help message.
exit /b 0
