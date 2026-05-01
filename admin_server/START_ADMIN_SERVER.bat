@echo off
:: ================================================================
:: EDTMRS - Start Admin Server
:: Run this on the Admin PC
:: ================================================================
title EDTMRS Admin Server

echo.
echo  ================================================================
echo   EDTMRS Admin Server
echo  ================================================================
echo.

:: Find Python
where python >nul 2>&1
if %errorLevel% neq 0 (
    echo  ERROR: Python not found! Install Python 3.11+ from python.org
    pause & exit /b 1
)

cd /d "%~dp0"

:: Install dependencies if needed
echo  Checking dependencies...
pip install -r requirements.txt -q
echo  Dependencies OK

:: Show Admin PC IP
echo.
echo  Your Admin PC IP addresses:
ipconfig | findstr "IPv4"
echo.
echo  Share one of these IPs with User PCs for their config.ini
echo.

:: Delete old DB to start fresh (comment this line after first run)
:: del edtmrs.db 2>nul

echo  Starting EDTMRS Admin Server on port 8000...
echo  Dashboard: http://localhost:3000
echo  Login: admin / Admin@1234
echo.
echo  Press Ctrl+C to stop
echo  ----------------------------------------------------------------
python main.py
pause
