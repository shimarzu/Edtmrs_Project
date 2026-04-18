@echo off
:: EDTMRS - Agent Service Uninstaller
:: Run as Administrator to completely remove the service

echo.
echo  ============================================
echo   EDTMRS Endpoint Agent - Service Uninstaller
echo  ============================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo  ERROR: Please run as Administrator!
    pause
    exit /b 1
)

echo  Stopping EDTMRS Agent service...
net stop EDTMRSAgent >nul 2>&1

echo  Removing EDTMRS Agent service...
sc delete EDTMRSAgent >nul 2>&1

if %errorLevel% equ 0 (
    echo  Service removed successfully.
) else (
    echo  Service was not found or already removed.
)

echo.
echo  EDTMRS Agent has been uninstalled.
echo.
pause
