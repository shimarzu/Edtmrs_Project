"""
EDTMRS - WebSocket Manager
Real-time alert broadcasting to connected admin dashboards
"""

import json
import logging
from typing import List
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

logger = logging.getLogger(__name__)

ws_router = APIRouter()


class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"WebSocket client connected. Total: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
        logger.info(f"WebSocket client disconnected. Total: {len(self.active_connections)}")

    async def broadcast(self, message: dict):
        """Send message to ALL connected admin dashboards."""
        if not self.active_connections:
            return
        text = json.dumps(message)
        dead = []
        for connection in self.active_connections:
            try:
                await connection.send_text(text)
            except Exception:
                dead.append(connection)
        for conn in dead:
            self.disconnect(conn)

    async def send_alert(self, alert_data: dict):
        """Send a device alert event."""
        await self.broadcast({
            "type": "device_alert",
            "data": alert_data
        })

    async def send_device_event(self, event_data: dict):
        """Send a device connected event."""
        await self.broadcast({
            "type": "device_event",
            "data": event_data
        })

    async def send_endpoint_update(self, endpoint_data: dict):
        """Send endpoint status update."""
        await self.broadcast({
            "type": "endpoint_update",
            "data": endpoint_data
        })


# Global manager instance
manager = ConnectionManager()


@ws_router.websocket("/ws/alerts")
async def websocket_alerts(websocket: WebSocket):
    """WebSocket endpoint for real-time admin alerts."""
    await manager.connect(websocket)
    try:
        # Send welcome message
        await websocket.send_text(json.dumps({
            "type": "connected",
            "message": "EDTMRS WebSocket connected. Listening for alerts..."
        }))
        # Keep connection alive, listen for pings
        while True:
            data = await websocket.receive_text()
            if data == "ping":
                await websocket.send_text(json.dumps({"type": "pong"}))
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
        manager.disconnect(websocket)
