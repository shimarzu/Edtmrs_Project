@echo off
:: EDTMRS - Open Firewall Port on Admin PC
:: Run ONCE as Administrator on Admin PC
net session >nul 2>&1
if %errorLevel% neq 0 ( echo Run as Administrator! & pause & exit /b 1 )
echo Opening port 8000 for EDTMRS...
netsh advfirewall firewall add rule name="EDTMRS Server Port 8000" ^
    dir=in action=allow protocol=TCP localport=8000
echo Port 8000 opened. User PCs can now connect.
pause
