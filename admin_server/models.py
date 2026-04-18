"""
EDTMRS - Pydantic Models / Schemas
"""

from pydantic import BaseModel, EmailStr
from typing import Optional
from datetime import datetime


class LoginRequest(BaseModel):
    username: str
    password: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    user: dict


class DeviceEventPayload(BaseModel):
    vendor_id: Optional[str] = "unknown"
    product_id: Optional[str] = "unknown"
    serial_number: Optional[str] = "unknown"
    device_name: Optional[str] = "Unknown Device"
    device_type: Optional[str] = "USB"
    drive_letter: Optional[str] = ""
    hostname: str
    username: str
    timestamp: Optional[str] = None
    agent_version: Optional[str] = "1.0.0"
    ip_address: Optional[str] = ""


class BlockDeviceRequest(BaseModel):
    vendor_id: Optional[str] = None
    product_id: Optional[str] = None
    serial_number: Optional[str] = None
    device_name: Optional[str] = None
    reason: Optional[str] = "Blocked by administrator"


class WhitelistDeviceRequest(BaseModel):
    vendor_id: Optional[str] = None
    product_id: Optional[str] = None
    serial_number: Optional[str] = None
    device_name: Optional[str] = None
    notes: Optional[str] = ""


class IsolateEndpointRequest(BaseModel):
    endpoint_id: int
    reason: Optional[str] = "Isolated by administrator"


class AcknowledgeAlertRequest(BaseModel):
    alert_id: int


class CreateUserRequest(BaseModel):
    username: str
    email: str
    password: str
    role: str = "admin"
