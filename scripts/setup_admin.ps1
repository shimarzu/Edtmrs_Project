# EDTMRS - Admin PC Setup Script
# Run this on the ADMIN PC (PowerShell as Administrator)
# Usage: .\setup_admin.ps1

Write-Host ""
Write-Host "  EDTMRS Admin Server Setup" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────" -ForegroundColor DarkGray
Write-Host ""

# ── 1. Check Python ────────────────────────────────────────────────────────────
Write-Host "[1/6] Checking Python installation..." -ForegroundColor Yellow
try {
    $pyVersion = python --version 2>&1
    Write-Host "  ✅ $pyVersion" -ForegroundColor Green
} catch {
    Write-Host "  ❌ Python not found. Install Python 3.11+ from https://python.org" -ForegroundColor Red
    exit 1
}

# ── 2. Check Node.js ───────────────────────────────────────────────────────────
Write-Host "[2/6] Checking Node.js installation..." -ForegroundColor Yellow
try {
    $nodeVersion = node --version 2>&1
    Write-Host "  ✅ Node.js $nodeVersion" -ForegroundColor Green
} catch {
    Write-Host "  ❌ Node.js not found. Install from https://nodejs.org (v18+)" -ForegroundColor Red
    exit 1
}

# ── 3. Install Python dependencies ────────────────────────────────────────────
Write-Host "[3/6] Installing Python dependencies..." -ForegroundColor Yellow
Set-Location "$PSScriptRoot\..\admin_server"
pip install -r requirements.txt --quiet
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ❌ pip install failed!" -ForegroundColor Red
    exit 1
}
Write-Host "  ✅ Python packages installed" -ForegroundColor Green

# ── 4. Install React dependencies ─────────────────────────────────────────────
Write-Host "[4/6] Installing React dashboard dependencies..." -ForegroundColor Yellow
Set-Location "$PSScriptRoot\..\dashboard"
npm install --silent
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ❌ npm install failed!" -ForegroundColor Red
    exit 1
}
Write-Host "  ✅ Node packages installed" -ForegroundColor Green

# ── 5. Get local IP ───────────────────────────────────────────────────────────
Write-Host "[5/6] Detecting Admin PC IP address..." -ForegroundColor Yellow
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.*" } | Select-Object -First 1).IPAddress
Write-Host "  ✅ Admin PC IP: $ip" -ForegroundColor Green

# ── 6. Create .env for React ──────────────────────────────────────────────────
Write-Host "[6/6] Creating React environment config..." -ForegroundColor Yellow
$envContent = "REACT_APP_API_URL=http://${ip}:8000"
Set-Content -Path "$PSScriptRoot\..\dashboard\.env" -Value $envContent
Write-Host "  ✅ Created dashboard/.env with REACT_APP_API_URL=http://${ip}:8000" -ForegroundColor Green

Write-Host ""
Write-Host "  ✅ Setup Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "  ─── HOW TO START ──────────────────────────────────────" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  Terminal 1 — Start FastAPI backend:" -ForegroundColor Cyan
Write-Host "    cd admin_server" -ForegroundColor White
Write-Host "    python main.py" -ForegroundColor White
Write-Host ""
Write-Host "  Terminal 2 — Start React dashboard:" -ForegroundColor Cyan
Write-Host "    cd dashboard" -ForegroundColor White
Write-Host "    npm start" -ForegroundColor White
Write-Host ""
Write-Host "  Dashboard URL : http://localhost:3000" -ForegroundColor Green
Write-Host "  API URL       : http://${ip}:8000" -ForegroundColor Green
Write-Host "  Default login : admin / Admin@1234" -ForegroundColor Yellow
Write-Host ""
Write-Host "  ─── USER PC CONFIG ───────────────────────────────────" -ForegroundColor DarkGray
Write-Host "  On each User PC, edit endpoint_agent/config.ini:" -ForegroundColor White
Write-Host "    SERVER_HOST=$ip" -ForegroundColor Yellow
Write-Host ""
