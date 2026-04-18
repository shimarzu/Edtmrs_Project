@echo off
:: EDTMRS - Agent Service Installer
:: Run this as Administrator ONCE on the User PC
:: After this, the agent starts automatically every time Windows boots
:: The agent runs silently - no console window visible to the user

echo.
echo  ============================================
echo   EDTMRS Endpoint Agent - Service Installer
echo   SLIIT Cybersecurity Project
echo  ============================================
echo.

:: Check if running as Administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo  ERROR: Please run this script as Administrator!
    echo  Right-click install_agent.bat and select "Run as administrator"
    pause
    exit /b 1
)

:: Get the directory where this batch file is located
set AGENT_DIR=%~dp0
set AGENT_EXE=%AGENT_DIR%edtmrs_agent.exe

:: Check if exe exists
if not exist "%AGENT_EXE%" (
    echo  ERROR: edtmrs_agent.exe not found in %AGENT_DIR%
    echo  Please compile the agent first, then run this installer.
    pause
    exit /b 1
)

echo  Agent found: %AGENT_EXE%
echo.

:: Stop existing service if running
echo  Checking for existing service...
sc query EDTMRSAgent >nul 2>&1
if %errorLevel% equ 0 (
    echo  Stopping existing service...
    net stop EDTMRSAgent >nul 2>&1
    echo  Removing existing service...
    sc delete EDTMRSAgent >nul 2>&1
    timeout /t 2 /nobreak >nul
)

:: Install the service
echo  Installing EDTMRS Agent as Windows Service...
sc create EDTMRSAgent ^
    binPath= "\"%AGENT_EXE%\" --service" ^
    DisplayName= "EDTMRS Endpoint Security Agent" ^
    start= auto ^
    type= own ^
    error= normal

if %errorLevel% neq 0 (
    echo  ERROR: Failed to install service! Error code: %errorLevel%
    pause
    exit /b 1
)

:: Set service description
sc description EDTMRSAgent "EDTMRS External Device Threat Monitoring Agent - Monitors USB device connections and reports to the security dashboard."

:: Set service to restart automatically if it crashes
sc failure EDTMRSAgent reset= 86400 actions= restart/5000/restart/10000/restart/30000

:: Start the service now
echo.
echo  Starting service...
net start EDTMRSAgent

if %errorLevel% neq 0 (
    echo  WARNING: Service installed but failed to start immediately.
    echo  It will start automatically on next reboot.
) else (
    echo  Service started successfully!
)

echo.
echo  ============================================
echo   Installation Complete!
echo  ============================================
echo.
echo  The EDTMRS agent will now:
echo   - Start automatically when Windows boots
echo   - Run silently in the background
echo   - Monitor all USB device connections
echo   - Send alerts to the admin dashboard
echo.
echo  To verify: Open Task Manager ^> Services tab
echo  Look for: EDTMRSAgent  (Status: Running)
echo.
echo  To uninstall: Run uninstall_agent.bat as Administrator
echo.
pause
