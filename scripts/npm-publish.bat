@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\

set SKIP_BUILD=0
set DRY_RUN=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--skip-build" (
    set SKIP_BUILD=1
    shift
    goto parse_args
)
if /i "%~1"=="--dry-run" (
    set DRY_RUN=1
    shift
    goto parse_args
)
if /i "%~1"=="--help" goto show_help_ok
if /i "%~1"=="-h" goto show_help_ok
echo Unknown option: %~1
goto show_help
:args_done

echo === Numkit - Publish npm package (manual) ===
echo Package: %PROJECT_DIR%packages\numkit
if "%DRY_RUN%"=="1" echo Mode: DRY RUN (no upload to the registry)
echo.

where node >nul 2>&1
if errorlevel 1 (
    echo ERROR: Node.js not found. Install from https://nodejs.org/
    exit /b 1
)
where npm >nul 2>&1
if errorlevel 1 (
    echo ERROR: npm not found on PATH.
    exit /b 1
)

if "%SKIP_BUILD%"=="0" (
    echo === Step 1/4: Rebuilding the WASM engine ===
    call "%SCRIPT_DIR%web-build.bat"
    if errorlevel 1 (
        echo ERROR: web-build failed - aborting publish.
        exit /b 1
    )
) else (
    echo === Step 1/4: Skipping WASM build (--skip-build^)
)

echo.
echo === Step 2/4: Refreshing package dist/ from build output ===
pushd "%PROJECT_DIR%packages\numkit"
call node scripts\refresh-dist.js
if errorlevel 1 (
    popd
    echo ERROR: WASM artifacts missing - run without --skip-build first.
    exit /b 1
)

echo.
echo === Step 3/4: Smoke test (inline MATLAB eval through the CLI^) ===
call npm test
if errorlevel 1 (
    popd
    echo ERROR: CLI smoke test failed - aborting publish.
    exit /b 1
)

echo.
echo === Step 4/4: Pack preview ===
call npm pack --dry-run

if "%DRY_RUN%"=="1" (
    echo.
    echo === Dry run complete - nothing was published. ===
    echo Re-run without --dry-run to upload to the npm registry.
    popd
    exit /b 0
)

echo.
npm whoami >nul 2>&1
if errorlevel 1 (
    echo ERROR: Not logged in to npm. Run:  npm login
    popd
    exit /b 1
)

echo Publishing to the npm registry...
call npm publish --access public
if errorlevel 1 (
    popd
    echo ERROR: npm publish failed.
    exit /b 1
)

echo.
for /f "tokens=*" %%v in ('node -p "require('./package.json').version"') do set PKG_VERSION=%%v
popd
echo === Published numkit@%PKG_VERSION% ===
echo Verify: https://www.npmjs.com/package/numkit
echo Test:   npx numkit -e "disp(1+1)"
exit /b 0

:show_help_ok
echo Usage: %~nx0 [--skip-build] [--dry-run]
echo.
echo Publishes packages\numkit (the WASM CLI) to https://www.npmjs.com — manual flow.
echo.
echo Steps: rebuild WASM (web-build^) - refresh dist/ - npm test - npm publish.
echo Requires: Node 16+, npm login, EMSDK installed (unless --skip-build^).
echo.
echo Options:
echo   --skip-build   Reuse the existing build\browser\wasm artifacts.
echo   --dry-run      Do everything except the actual npm upload.
echo   -h, --help     Show this help message.
exit /b 0
:show_help
echo Usage: %~nx0 [--skip-build] [--dry-run]
echo Run with -h for details.
exit /b 1
