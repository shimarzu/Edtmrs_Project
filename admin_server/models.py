from pydantic import BaseModel
from typing import Optional, List

class LoginRequest(BaseModel):
    username: str
    password: str

class DeviceEventPayload(BaseModel):
    vendor_id:       str
    product_id:      str
    serial_number:   str
    device_name:     str
    device_type:     str = "USB Storage"
    drive_letter:    str = ""
    hostname:        str
    username:        str
    timestamp:       str
    agent_version:   str = "unknown"
    dangerous_files: List[str] = []
    file_count:      int = 0

class HeartbeatPayload(BaseModel):
    hostname:      str
    ip_address:    Optional[str] = ""
    username:      Optional[str] = ""
    agent_version: Optional[str] = "unknown"

class BlockDeviceRequest(BaseModel):
    vendor_id:     str
    product_id:    str
    serial_number: Optional[str] = "unknown"
    device_name:   Optional[str] = "Unknown Device"
    reason:        Optional[str] = "Blocked by administrator"

class WhitelistDeviceRequest(BaseModel):
    vendor_id:     str
    product_id:    str
    serial_number: Optional[str] = "unknown"
    device_name:   Optional[str] = "Unknown Device"
    notes:         Optional[str] = ""

class IsolateRequest(BaseModel):
    endpoint_id: int
    reason:      Optional[str] = "Isolated by administrator"

class AcknowledgeRequest(BaseModel):
    alert_id: int

class ActionResultPayload(BaseModel):
    log_id:        Optional[int] = None
    action_result: str
    hostname:      str
