@echo off
:: EDTMRS - Compile Agent
:: Run as Administrator
title EDTMRS Compile Agent
echo.
echo  ================================================================
echo   EDTMRS Agent Compiler
echo  ================================================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 ( echo Run as Administrator! & pause & exit /b 1 )

set "DIR=%~dp0"
cd /d "%DIR%"

where g++ >nul 2>&1
if %errorLevel% neq 0 (
    echo  ERROR: g++ compiler not found!
    echo  Install w64devkit from: https://github.com/skeeto/w64devkit/releases
    echo  Extract to C:\w64devkit and add C:\w64devkit\bin to PATH
    pause & exit /b 1
)

echo  Stopping old agent...
net stop EDTMRSAgent >nul 2>&1
taskkill /f /im edtmrs_agent.exe >nul 2>&1
timeout /t 2 /nobreak >nul
del edtmrs_agent.exe 2>nul

echo  Compiling...
g++ -std=c++17 -O2 -static -o edtmrs_agent.exe ^
    main.cpp device_monitor.cpp http_client.cpp blocker.cpp ^
    -lwinhttp -lsetupapi -lcfgmgr32 -lws2_32

if %errorLevel% neq 0 (
    echo  COMPILE FAILED - check errors above
    pause & exit /b 1
)

echo  Compiled successfully!
echo.
echo  Now run SETUP_USER_PC.bat to install as service
echo  Or just run: edtmrs_agent.exe (for testing in console)
echo.
pause
