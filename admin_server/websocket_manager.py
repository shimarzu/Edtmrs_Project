import json, logging
from typing import List
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

logger    = logging.getLogger(__name__)
ws_router = APIRouter()

class ConnectionManager:
    def __init__(self):
        self.connections: List[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.connections.append(ws)
        logger.info(f"WS client connected. Total={len(self.connections)}")

    def disconnect(self, ws: WebSocket):
        if ws in self.connections:
            self.connections.remove(ws)
        logger.info(f"WS client disconnected. Total={len(self.connections)}")

    async def broadcast(self, msg: dict):
        if not self.connections:
            return
        text = json.dumps(msg)
        dead = []
        for ws in self.connections:
            try:    await ws.send_text(text)
            except: dead.append(ws)
        for ws in dead:
            self.disconnect(ws)

    async def send_device_event(self, data: dict):
        await self.broadcast({"type": "device_event", "data": data})

    async def send_alert(self, data: dict):
        await self.broadcast({"type": "device_alert", "data": data})

manager = ConnectionManager()

@ws_router.websocket("/ws/alerts")
async def ws_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        await ws.send_text(json.dumps({"type": "connected", "message": "EDTMRS connected"}))
        while True:
            data = await ws.receive_text()
            if data == "ping":
                await ws.send_text(json.dumps({"type": "pong"}))
    except WebSocketDisconnect:
        manager.disconnect(ws)
    except Exception as e:
        logger.error(f"WS error: {e}")
        manager.disconnect(ws)
