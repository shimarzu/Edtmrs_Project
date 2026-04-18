"""
EDTMRS - External Device Threat Monitoring and Response System
Admin Server - Main Entry Point
"""

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from contextlib import asynccontextmanager

from database import init_db, create_default_admin
from api_routes import router
from websocket_manager import ws_router

import logging

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🚀 EDTMRS Admin Server starting...")
    await init_db()
    await create_default_admin()
    logger.info("✅ Database initialized.")
    logger.info("✅ EDTMRS Admin Server ready.")
    yield
    logger.info("🛑 EDTMRS Admin Server shutting down.")


app = FastAPI(
    title="EDTMRS Admin Server",
    description="External Device Threat Monitoring and Response System",
    version="4.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(router, prefix="/api")
app.include_router(ws_router)


@app.get("/")
async def root():
    return {
        "system": "EDTMRS",
        "version": "4.0.0",
        "status": "running",
        "message": "External Device Threat Monitoring and Response System"
    }


@app.get("/health")
async def health():
    return {"status": "healthy"}


if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=False,
        log_level="info"
    )
