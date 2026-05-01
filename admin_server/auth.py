import bcrypt, jwt, os, aiosqlite
from datetime import datetime, timedelta
from fastapi import Depends, HTTPException
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from database import DB_PATH

SECRET_KEY = os.environ.get("EDTMRS_SECRET", "edtmrs-secret-2024")
ALGORITHM  = "HS256"

security = HTTPBearer()

def hash_password(p: str) -> str:
    return bcrypt.hashpw(p.encode(), bcrypt.gensalt()).decode()

def verify_password(plain: str, hashed: str) -> bool:
    return bcrypt.checkpw(plain.encode(), hashed.encode())

def create_token(data: dict) -> str:
    payload = {**data, "exp": datetime.utcnow() + timedelta(hours=24)}
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

def decode_token(token: str) -> dict:
    try:
        return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token expired")
    except Exception:
        raise HTTPException(status_code=401, detail="Invalid token")

async def get_current_user(creds: HTTPAuthorizationCredentials = Depends(security)):
    data = decode_token(creds.credentials)
    username = data.get("username")
    if not username:
        raise HTTPException(status_code=401, detail="Invalid token")
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT id,username,email,role FROM users WHERE username=?", (username,))
        user = await cur.fetchone()
    if not user:
        raise HTTPException(status_code=401, detail="User not found")
    return dict(user)

async def require_admin(user=Depends(get_current_user)):
    if user["role"] not in ("admin","superadmin"):
        raise HTTPException(status_code=403, detail="Admin required")
    return user
