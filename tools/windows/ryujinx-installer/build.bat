@echo off
rem Build the installer. Usage: build.bat "C:\path\to\folder-with-Ryujinx.exe"   (or a zip of it)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Ryujinx %*
pause
