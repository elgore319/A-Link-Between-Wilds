@echo off
rem Double-click on the OLD PC to zip your Ryujinx data to the Desktop. Arguments pass through to the .ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0export-ryujinx-data.ps1" %*
pause
