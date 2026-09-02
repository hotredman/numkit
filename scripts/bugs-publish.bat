@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set BUGS_HTML_DIR=%PROJECT_DIR%build\bugs
set DEPLOY_GIT_DIR=%PROJECT_DIR%build\deploy-bugs

:: Default remote repository URL
set REPO_URL=git@github.com:hotredman/numkit-bugs.git
if not "%NUMKIT_BUGS_REPO%"=="" set REPO_URL=%NUMKIT_BUGS_REPO%

set DO_PUSH=1
set SKIP_BUILD=0
set DEST_DIR=

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
if /i "%~1"=="--repo" (
    set REPO_URL=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--dest" (
    set DEST_DIR=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help

set DEST_DIR=%~1
shift
goto parse_args
:args_done

echo === NumKit Bugs ^& Parity - Deploy to GitHub Pages ===
echo Source: %PROJECT_DIR%
if not "%DEST_DIR%"=="" (
    echo Target Local: %DEST_DIR%
) else (
    echo Target Remote: %REPO_URL%
    echo Workspace: %DEPLOY_GIT_DIR%
)
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

:: 3. Setup deploy workspace inside build/
if "%DEST_DIR%"=="" (
    set TARGET_DIR=%DEPLOY_GIT_DIR%
    if not exist "%DEPLOY_GIT_DIR%" mkdir "%DEPLOY_GIT_DIR%"
    cd /d "%DEPLOY_GIT_DIR%"
    if not exist "%DEPLOY_GIT_DIR%\.git" (
        git init -b main >nul 2>&1
        git remote add origin %REPO_URL% >nul 2>&1
    ) else (
        git remote set-url origin %REPO_URL% >nul 2>&1
    )
) else (
    set TARGET_DIR=%DEST_DIR%
)

echo Syncing bugs catalog files to deploy workspace...

:: Clean existing files in target workspace (except .git)
cd /d "%TARGET_DIR%"
for /f "delims=" %%F in ('dir /b /a-d 2^>nul') do (
    if not "%%F"==".git" del /f /q "%%F" 2>nul
)
for /f "delims=" %%D in ('dir /b /ad 2^>nul') do (
    if not "%%D"==".git" rd /s /q "%%D" 2>nul
)

:: Copy fresh files from build/bugs/ into target workspace
xcopy /e /i /y /q "%BUGS_HTML_DIR%\*" "%TARGET_DIR%\" >nul
type nul > "%TARGET_DIR%\.nojekyll"

echo Files synchronized.

:: 4. Clean single-commit history (Orphan Branch)
echo.
echo Creating clean 1-commit state...
git checkout --orphan temp_deploy >nul 2>&1 || git checkout -b temp_deploy >nul 2>&1
git add -A
git commit -m "docs(bugs): NumKit Defect & Parity Catalog (numkit@%SRC_REV%)" >nul 2>&1
git branch -D main >nul 2>&1
git branch -m main >nul 2>&1

if "%DO_PUSH%"=="1" (
    echo.
    echo Force-pushing single clean commit to %REPO_URL%...
    git push -f origin main
    if errorlevel 1 (
        echo.
        echo ERROR: git push failed! Please verify remote repository existence and SSH access: %REPO_URL%
        exit /b 1
    )
    echo.
    echo Successfully published clean 1-commit mirror to GitHub Pages!
    goto done
) else (
    echo.
    echo Clean commit created locally in %TARGET_DIR% (push skipped due to --no-push).
    goto done
)

:done
cd /d "%PROJECT_DIR%"
exit /b 0

:show_help
echo Usage: %~nx0 [options]
echo.
echo Options:
echo   --repo ^<url^>     Remote Git repository URL (default: git@github.com:hotredman/numkit-bugs.git)
echo   --dest ^<dir^>     Optional local destination directory
echo   --push           Force-push 1 clean commit to remote repository (default: on)
echo   --no-push        Create 1 clean commit locally in workspace without pushing
echo   --skip-build     Skip running build_bugs_site.py
echo   -h, --help       Show this help message
exit /b 0
