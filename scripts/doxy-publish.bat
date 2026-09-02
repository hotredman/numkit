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

echo === NumKit Doxygen - Deploy Clean Mirror to GitHub Pages ===
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

:: Clean existing files in target (except .git)
cd /d "%DOXY_DIR%"
for /f "delims=" %%F in ('dir /b /a-d 2^>nul') do (
    if not "%%F"==".git" del /f /q "%%F" 2>nul
)
for /f "delims=" %%D in ('dir /b /ad 2^>nul') do (
    if not "%%D"==".git" rd /s /q "%%D" 2>nul
)

:: Copy fresh files from build/docs/html/ into target repo
xcopy /e /i /y /q "%DOCS_HTML_DIR%\*" "%DOXY_DIR%\" >nul

:: Ensure .nojekyll exists
if not exist "%DOXY_DIR%\.nojekyll" (
    type nul > "%DOXY_DIR%\.nojekyll"
)

echo Files synchronized.

:: 3. Clean single-commit history (Orphan Branch)
echo.
echo Creating clean 1-commit state in Doxygen repository...
git checkout --orphan temp_deploy >nul 2>&1
git add -A
git commit -m "docs: NumKit C++ API Documentation (numkit@%SRC_REV%)" >nul 2>&1
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
    echo Successfully published clean 1-commit Doxygen docs to GitHub Pages!
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
echo   --skip-build     Skip re-running doxygen
echo   -h, --help       Show this help message
exit /b 0
