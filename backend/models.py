"""
Pydantic models for API request/response validation.
"""
from pydantic import BaseModel
from typing import Optional, List
from datetime import datetime


class DetectionResult(BaseModel):
    """Detection result from YOLO analysis."""
    id: int
    device_id: str
    animal: str
    confidence: float
    image_path: str
    timestamp: datetime

    class Config:
        from_attributes = True


class DeviceStatus(BaseModel):
    """Device status information."""
    device_id: str
    last_seen: Optional[datetime]
    status: str

    class Config:
        from_attributes = True


class AlertMessage(BaseModel):
    """Alert notification."""
    id: int
    device_id: str
    message: str
    timestamp: datetime

    class Config:
        from_attributes = True


class UploadResponse(BaseModel):
    """Response after image upload and detection."""
    success: bool
    message: str
    detection: Optional[dict] = None


class DashboardStats(BaseModel):
    """Dashboard summary statistics."""
    total_detections: int
    active_devices: int
    total_alerts: int
    recent_detections: List[DetectionResult]
