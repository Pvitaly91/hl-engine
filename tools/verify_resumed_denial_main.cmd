@echo off
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify_resumed_denial_main.ps1" %*
