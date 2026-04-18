#!/usr/bin/env python3
"""
EDTMRS Quick Start — Admin Server
Run: python start_server.py

Works on Windows, Linux, macOS
"""

import subprocess, sys, os, socket, pathlib

BASE = pathlib.Path(__file__).parent

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

def install_deps():
    req = BASE / "admin_server" / "requirements.txt"
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", str(req), "-q"])

def main():
    print("\n  EDTMRS Admin Server")
    print("  ─────────────────────────────────")

    ip = get_local_ip()
    print(f"\n  Admin PC IP : {ip}")
    print(f"  API         : http://{ip}:8000")
    print(f"  Dashboard   : http://localhost:3000")
    print(f"  Default     : admin / Admin@1234")
    print(f"\n  Configure User PCs with: SERVER_HOST={ip}")
    print("\n  Installing Python dependencies...")

    try:
        install_deps()
        print("  ✅ Dependencies ready\n")
    except Exception as e:
        print(f"  ⚠ pip error: {e}")

    os.chdir(BASE / "admin_server")
    print("  Starting FastAPI server on 0.0.0.0:8000 ...")
    print("  Press Ctrl+C to stop.\n")

    os.execv(sys.executable, [sys.executable, "-m", "uvicorn",
        "main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"])

if __name__ == "__main__":
    main()
