# EDTMRS - User PC / Endpoint Agent Build Script
# Run this on the USER PC (PowerShell as Administrator)
# Usage: .\build_agent.ps1 -ServerIP "192.168.1.20"

param(
    [string]$ServerIP = "192.168.1.20",
    [int]$ServerPort  = 8000
)

Write-Host ""
Write-Host "  EDTMRS Endpoint Agent Builder" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────────" -ForegroundColor DarkGray
Write-Host "  Target Server: ${ServerIP}:${ServerPort}" -ForegroundColor Yellow
Write-Host ""

$agentDir = "$PSScriptRoot\..\endpoint_agent"

# ── Update config.ini ──────────────────────────────────────────────────────────
Write-Host "[1/3] Writing config.ini..." -ForegroundColor Yellow
$configContent = @"
# EDTMRS Endpoint Agent Configuration
SERVER_HOST=$ServerIP
SERVER_PORT=$ServerPort
HEARTBEAT_INTERVAL=30
"@
Set-Content -Path "$agentDir\config.ini" -Value $configContent
Write-Host "  ✅ config.ini written: SERVER_HOST=$ServerIP" -ForegroundColor Green

# ── Check for compiler ────────────────────────────────────────────────────────
Write-Host "[2/3] Checking build tools..." -ForegroundColor Yellow

$hasMSVC  = Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
$hasMinGW = Get-Command g++ -ErrorAction SilentlyContinue

if ($hasMinGW) {
    # ── Build with MinGW (simple, no CMake needed) ────────────────────────────
    Write-Host "[3/3] Building with MinGW..." -ForegroundColor Yellow
    Set-Location $agentDir

    g++ -std=c++17 -O2 -o edtmrs_agent.exe main.cpp device_monitor.cpp http_client.cpp -lwinhttp -lsetupapi -lcfgmgr32 -lws2_32

    if (Test-Path "$agentDir\edtmrs_agent.exe") {
        Write-Host "  ✅ Built: endpoint_agent\edtmrs_agent.exe" -ForegroundColor Green
    } else {
        Write-Host "  ❌ Build failed!" -ForegroundColor Red
    }
} elseif ($hasMSVC) {
    # ── Build with MSVC ───────────────────────────────────────────────────────
    Write-Host "[3/3] Building with MSVC..." -ForegroundColor Yellow
    Set-Location $agentDir

    # Use cl.exe directly (no CMake needed)
    $clPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    & $clPath amd64
    cl.exe /EHsc /O2 /std:c++17 main.cpp device_monitor.cpp http_client.cpp /link winhttp.lib setupapi.lib cfgmgr32.lib ws2_32.lib /OUT:edtmrs_agent.exe

    if (Test-Path "$agentDir\edtmrs_agent.exe") {
        Write-Host "  ✅ Built: endpoint_agent\edtmrs_agent.exe" -ForegroundColor Green
    } else {
        Write-Host "  ❌ Build failed!" -ForegroundColor Red
    }
} else {
    Write-Host "  ⚠ No compiler found." -ForegroundColor Yellow
    Write-Host "  Install options:" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION A — Visual Studio 2022 (recommended):" -ForegroundColor Cyan
    Write-Host "    https://visualstudio.microsoft.com/" -ForegroundColor DarkGray
    Write-Host "    Install 'Desktop development with C++' workload" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION B — MinGW/w64devkit (lightweight):" -ForegroundColor Cyan
    Write-Host "    https://github.com/skeeto/w64devkit/releases" -ForegroundColor DarkGray
    Write-Host "    Extract and add bin\ to PATH" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION C — Manual MinGW compile after installing:" -ForegroundColor Cyan
    Write-Host "    cd endpoint_agent" -ForegroundColor White
    Write-Host '    g++ -std=c++17 -O2 -o edtmrs_agent.exe main.cpp device_monitor.cpp http_client.cpp -lwinhttp -lsetupapi -lcfgmgr32 -lws2_32' -ForegroundColor White
    Write-Host ""
}

Write-Host ""
Write-Host "  ─── HOW TO RUN THE AGENT ──────────────────────────────" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  A) Run in foreground (console visible, good for testing):" -ForegroundColor Cyan
Write-Host "    .\endpoint_agent\edtmrs_agent.exe" -ForegroundColor White
Write-Host ""
Write-Host "  B) Install as Windows background service:" -ForegroundColor Cyan
Write-Host "    .\endpoint_agent\edtmrs_agent.exe --install-service" -ForegroundColor White
Write-Host "    net start EDTMRSAgent" -ForegroundColor White
Write-Host ""
Write-Host "  C) Remove service:" -ForegroundColor Cyan
Write-Host "    net stop EDTMRSAgent" -ForegroundColor White
Write-Host "    .\endpoint_agent\edtmrs_agent.exe --remove-service" -ForegroundColor White
Write-Host ""
