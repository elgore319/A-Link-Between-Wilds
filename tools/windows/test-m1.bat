@echo off
rem Double-click to run the in-game test. Arguments are passed through to test-m1.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-m1.ps1" %*
pause
