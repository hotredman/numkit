@echo off
setlocal
cd /d "%~dp0..\.."
python tools\deploy_pages.py --type bugs %*
exit /b %ERRORLEVEL%
