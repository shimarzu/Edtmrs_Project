@echo off
:: ================================================================
:: EDTMRS - Start Dashboard
:: Run this on the Admin PC
:: ================================================================
title EDTMRS Dashboard

cd /d "%~dp0"
echo.
echo  Starting EDTMRS Dashboard...
echo  Opening at http://localhost:3000
echo.
npm start
pause
