@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0release\setup_player.ps1" %*
if errorlevel 1 pause
