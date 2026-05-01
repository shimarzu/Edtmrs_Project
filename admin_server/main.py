import uvicorn, logging
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from database import init_db, create_default_admin
from api_routes import router
from websocket_manager import ws_router

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

@asynccontextmanager
async def lifespan(app: FastAPI):
    await init_db()
    await create_default_admin()
    yield

app = FastAPI(title="EDTMRS", version="6.1.0", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"],
    allow_credentials=True, allow_methods=["*"], allow_headers=["*"])
app.include_router(router)
app.include_router(ws_router)

@app.get("/")
async def root():
    return {"system":"EDTMRS","version":"6.1.0","status":"running"}

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=False)
