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

# ── Check for CMake + compiler ────────────────────────────────────────────────
Write-Host "[2/3] Checking build tools..." -ForegroundColor Yellow

$hasCmake = Get-Command cmake -ErrorAction SilentlyContinue
$hasMSVC  = Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
$hasMinGW = Get-Command g++ -ErrorAction SilentlyContinue

if ($hasCmake -and ($hasMSVC -or $hasMinGW)) {
    Write-Host "  ✅ CMake and compiler found" -ForegroundColor Green

    # ── Build with CMake ──────────────────────────────────────────────────────
    Write-Host "[3/3] Building with CMake..." -ForegroundColor Yellow
    $buildDir = "$agentDir\build"
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    Set-Location $buildDir
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
    cmake --build . --config Release

    if (Test-Path "$buildDir\Release\edtmrs_agent.exe") {
        Copy-Item "$buildDir\Release\edtmrs_agent.exe" "$agentDir\edtmrs_agent.exe" -Force
        Write-Host "  ✅ Built: endpoint_agent\edtmrs_agent.exe" -ForegroundColor Green
    } elseif (Test-Path "$buildDir\edtmrs_agent.exe") {
        Copy-Item "$buildDir\edtmrs_agent.exe" "$agentDir\edtmrs_agent.exe" -Force
        Write-Host "  ✅ Built: endpoint_agent\edtmrs_agent.exe" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ Build may have succeeded — check $buildDir" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⚠ CMake or compiler not found." -ForegroundColor Yellow
    Write-Host "  Manual build options:" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION A — Visual Studio (recommended):" -ForegroundColor Cyan
    Write-Host "    1. Install Visual Studio 2022 Community (free)" -ForegroundColor White
    Write-Host "       https://visualstudio.microsoft.com/" -ForegroundColor DarkGray
    Write-Host "    2. Install 'Desktop development with C++' workload" -ForegroundColor White
    Write-Host "    3. Install CMake from https://cmake.org" -ForegroundColor White
    Write-Host "    4. Re-run this script" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION B — MinGW (lightweight, no Visual Studio):" -ForegroundColor Cyan
    Write-Host "    1. Download w64devkit: https://github.com/skeeto/w64devkit/releases" -ForegroundColor White
    Write-Host "    2. Extract and add its bin\ folder to PATH" -ForegroundColor White
    Write-Host "    3. Run manual compile command below" -ForegroundColor White
    Write-Host ""
    Write-Host "  OPTION C — Manual MinGW compile:" -ForegroundColor Cyan
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
