@echo off
title MyWinApps Installer
echo Launching installer with administrative privileges...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup.ps1"
