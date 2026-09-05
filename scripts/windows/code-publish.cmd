@echo off
rem Push the local main branch to the public GitHub mirror (hotredman/numkit).
rem Usage: scripts\windows\code-publish.cmd [--force]
setlocal
cd /d "%~dp0..\.."
git push github main %*
exit /b %ERRORLEVEL%
