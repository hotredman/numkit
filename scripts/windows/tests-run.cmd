@echo off
setlocal

cd /d "%~dp0..\.."

echo === Building test target (nk_gtest) ===
if not exist "build\windows\release\CMakeCache.txt" (
    echo Configuring windows-release preset...
    cmake --preset=windows-release
    if errorlevel 1 (
        echo ERROR: Configuration failed!
        exit /b 1
    )
)
cmake --build --preset=windows-release --target nk_gtest
if errorlevel 1 (
    echo ERROR: Build of nk_gtest failed!
    exit /b 1
)

if "%1"=="--build-only" (
    echo Build completed successfully: --build-only specified.
    exit /b 0
)

set TEST_EXE=build\windows\release\tests\gtest\Release\nk_gtest.exe
if not exist "%TEST_EXE%" set TEST_EXE=build\windows\release\tests\gtest\nk_gtest.exe

if not exist "%TEST_EXE%" (
    echo ERROR: Could not find test runner at %TEST_EXE%
    exit /b 1
)

echo.
echo === Running tests ===
"%TEST_EXE%" %*
exit /b %ERRORLEVEL%
