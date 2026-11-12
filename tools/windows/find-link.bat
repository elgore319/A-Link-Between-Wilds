@echo off
rem Double-click to find where BotW 1.0.0 keeps Link's position (the player scan).
rem Load your save, then run in a straight line until this window says FOUND.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-m1.ps1" -Scan %*
pause
