@echo off
:: ================================================================
:: EDTMRS - User PC Setup
:: Run this ONCE on each User PC as Administrator
:: This installs the monitoring agent that starts automatically
:: ================================================================
title EDTMRS User PC Setup

echo.
echo  ================================================================
echo   EDTMRS - External Device Threat Monitoring System
echo   User PC Setup Wizard
echo  ================================================================
echo.

:: Check Administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo  ERROR: Please right-click this file and select
    echo         "Run as administrator"
    echo.
    pause & exit /b 1
)

set "AGENT_DIR=%~dp0"
set "AGENT_EXE=%AGENT_DIR%edtmrs_agent.exe"

:: Check exe exists
if not exist "%AGENT_EXE%" (
    echo  ERROR: edtmrs_agent.exe not found!
    echo  Please compile first using compile_and_install.bat
    echo.
    pause & exit /b 1
)

echo  Step 1: Enter your ADMIN PC IP Address
echo  (On Admin PC: open PowerShell and run ipconfig)
echo  (Look for IPv4 Address under WiFi or Ethernet)
echo.
set /p ADMIN_IP="  Admin PC IP Address: "

if "%ADMIN_IP%"=="" (
    echo  ERROR: IP address cannot be empty
    pause & exit /b 1
)

:: Write config.ini
echo # EDTMRS Configuration > "%AGENT_DIR%config.ini"
echo SERVER_HOST=%ADMIN_IP% >> "%AGENT_DIR%config.ini"
echo SERVER_PORT=8000 >> "%AGENT_DIR%config.ini"
echo HEARTBEAT_INTERVAL=30 >> "%AGENT_DIR%config.ini"
echo.
echo  Config saved: Server = %ADMIN_IP%:8000

:: Test connection to admin server
echo.
echo  Step 2: Testing connection to Admin PC...
curl -s --connect-timeout 5 http://%ADMIN_IP%:8000/health >nul 2>&1
if %errorLevel% equ 0 (
    echo  Connection SUCCESS - Admin server is reachable!
) else (
    echo  WARNING: Cannot reach Admin PC at %ADMIN_IP%:8000
    echo  Make sure:
    echo    1. Admin PC server is running (python main.py)
    echo    2. Both PCs are on the same network
    echo    3. Windows Firewall allows port 8000 on Admin PC
    echo.
    choice /C YN /M "Continue anyway?"
    if errorlevel 2 exit /b 1
)

:: Stop and remove old service if exists
echo.
echo  Step 3: Installing Windows Service...
net stop EDTMRSAgent >nul 2>&1
sc delete EDTMRSAgent >nul 2>&1
timeout /t 2 /nobreak >nul

:: Install service
sc create EDTMRSAgent ^
    binPath= "\"%AGENT_EXE%\" --service" ^
    DisplayName= "EDTMRS Endpoint Security Agent" ^
    start= auto ^
    type= own
sc description EDTMRSAgent "EDTMRS USB Device Threat Monitoring - monitors USB connections and reports to security dashboard"
sc failure EDTMRSAgent reset= 86400 actions= restart/5000/restart/10000/restart/30000

:: Start service
net start EDTMRSAgent
if %errorLevel% equ 0 (
    echo  Service started successfully!
) else (
    echo  Service will start on next reboot.
)

echo.
echo  ================================================================
echo   SETUP COMPLETE on %COMPUTERNAME%
echo  ================================================================
echo.
echo  This PC is now monitored by EDTMRS.
echo  - Agent starts automatically on every boot
echo  - USB devices will be reported to %ADMIN_IP%
echo  - Check dashboard at http://%ADMIN_IP%:3000
echo.
echo  To verify: Task Manager > Services > EDTMRSAgent > Running
echo.
pause
