@echo off
:: EDTMRS - Uninstall Agent from User PC
net session >nul 2>&1
if %errorLevel% neq 0 ( echo Run as Administrator! & pause & exit /b 1 )
echo Stopping and removing EDTMRS Agent...
net stop EDTMRSAgent >nul 2>&1
sc delete EDTMRSAgent >nul 2>&1
echo Done. Agent removed from this PC.
pause
