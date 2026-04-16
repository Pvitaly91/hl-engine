@echo off
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0manual_verify_resumed_denial.ps1" %*
