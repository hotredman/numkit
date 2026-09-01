@echo off
rem Push the local main to the PUBLIC GitHub mirror (hotredman/numkit).
rem Per AGENTS.md push policy: the agent pushes only to origin
rem (git.megahard.ru); this mirror is pushed manually by the user.
rem
rem Usage: scripts\push-mirror.bat [--force]

setlocal
git push github main %*
