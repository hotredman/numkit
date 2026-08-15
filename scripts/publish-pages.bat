@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set DEPLOY_DIR=%PROJECT_DIR%deploy
set DEFAULT_PAGES_DIR=%PROJECT_DIR%..\..\czssgkavo\numkit

:: Parse arguments
set PAGES_DIR=%DEFAULT_PAGES_DIR%
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
if /i "%~1"=="--help" (
    echo Usage: %~nx0 [--push] [--skip-build] [--dest ^<path^>]
    echo.
    echo Synchronizes the static Web IDE bundle ^(deploy\^) into the GitHub Pages repository.
    echo.
    echo Options:
    echo   --push        Automatically push commit to origin main in the Pages repo.
    echo   --skip-build  Skip re-running web-build.bat.
    echo   --dest ^<path^> Destination directory ^(default: ..\czssgkavo\numkit^).
    exit /b 0
)
set PAGES_DIR=%~1
shift
goto parse_args
:args_done

echo === Numkit Web IDE - Deploy to GitHub Pages Repository ===
echo Source: %PROJECT_DIR%
echo Target: %PAGES_DIR%
echo.

:: 1. Validate target directory
if not exist "%PAGES_DIR%\.git" (
    echo ERROR: Target directory is not a Git repository: "%PAGES_DIR%"
    echo Please make sure the repository is cloned, or specify path with --dest ^<path^>.
    exit /b 1
)

:: 2. Build web bundle if needed
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

:: 3. Get source git commit hash
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

:: 4. Check git status in target repo
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
git commit -m "Update Web IDE build (synced from megahard/numkit@%SRC_REV%)"

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
echo === Done! ===
