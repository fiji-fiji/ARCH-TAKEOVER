@echo off
rem Double-click to play the Saint-Martin map (no editor). Esc / Alt+F4 to quit.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\ue\play.ps1" %*
