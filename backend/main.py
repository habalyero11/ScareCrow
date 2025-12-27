"""
ScareCrowWeb Backend - FastAPI Application v2.0

Main application file handling all API endpoints for:
- Image upload from ESP32-CAM devices
- Detection logs retrieval
- Device status monitoring
- Alert management
- mDNS advertisement for device discovery
"""
from fastapi import FastAPI, File, UploadFile, Form, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse, JSONResponse
from contextlib import asynccontextmanager
from datetime import datetime
from typing import Optional
import os
import shutil
import uuid
import socket
import threading

# mDNS support
try:
    from zeroconf import ServiceInfo, Zeroconf
    MDNS_AVAILABLE = True
except ImportError:
    MDNS_AVAILABLE = False
    print("⚠️ Zeroconf not installed. mDNS will be disabled.")
    print("   Install with: pip install zeroconf")

from database import (
    init_database, 
    upsert_device, 
    save_detection, 
    save_alert,
    get_all_detections,
    get_all_devices,
    get_all_alerts,
    get_detection_count,
    get_active_device_count,
    get_alert_count,
    get_recent_detections,
    delete_alert,
    get_or_create_config,
    update_device_config
)
from detection import detect_animals, get_primary_detection, load_model


# Create uploads directory
UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)

# mDNS Configuration
MDNS_SERVICE_NAME = "ScareCrow Server"
MDNS_SERVICE_TYPE = "_scarecrow._tcp.local."
MDNS_HOSTNAME = "scarecrow"
MDNS_PORT = 8000

# Global mDNS objects
zeroconf_instance = None
mdns_service_info = None


def get_local_ip():
    """Get the local IP address of this machine."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"


def start_mdns():
    """Start mDNS advertisement."""
    global zeroconf_instance, mdns_service_info
    
    if not MDNS_AVAILABLE:
        return False
    
    try:
        local_ip = get_local_ip()
        print(f"📡 Starting mDNS advertisement...")
        print(f"   Local IP: {local_ip}")
        
        zeroconf_instance = Zeroconf()
        
        # Create service info
        mdns_service_info = ServiceInfo(
            MDNS_SERVICE_TYPE,
            f"{MDNS_SERVICE_NAME}.{MDNS_SERVICE_TYPE}",
            addresses=[socket.inet_aton(local_ip)],
            port=MDNS_PORT,
            properties={
                "version": "2.0",
                "path": "/",
            },
            server=f"{MDNS_HOSTNAME}.local.",
        )
        
        zeroconf_instance.register_service(mdns_service_info)
        print(f"✅ mDNS: Server available at http://{MDNS_HOSTNAME}.local:{MDNS_PORT}")
        return True
        
    except Exception as e:
        print(f"⚠️ mDNS registration failed: {e}")
        return False


def stop_mdns():
    """Stop mDNS advertisement."""
    global zeroconf_instance, mdns_service_info
    
    if zeroconf_instance and mdns_service_info:
        try:
            zeroconf_instance.unregister_service(mdns_service_info)
            zeroconf_instance.close()
            print("📡 mDNS service stopped")
        except:
            pass


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan events."""
    # Startup
    print("🚀 Starting ScareCrowWeb Backend v2.0...")
    init_database()
    load_model()  # Pre-load YOLO model
    
    # Start mDNS in a thread (doesn't block)
    mdns_started = start_mdns()
    
    print("✅ Backend ready!")
    if mdns_started:
        print(f"🌐 Dashboard: http://scarecrow.local:{MDNS_PORT}")
    print(f"🌐 Dashboard: http://{get_local_ip()}:{MDNS_PORT}")
    
    yield
    
    # Shutdown
    stop_mdns()
    print("👋 Shutting down ScareCrowWeb Backend...")


app = FastAPI(
    title="ScareCrowWeb API",
    description="Backend API for ESP32-CAM animal detection system",
    version="1.0.0",
    lifespan=lifespan
)

# CORS Configuration - Allow frontend access
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, specify exact origins
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Serve uploaded images
app.mount("/uploads", StaticFiles(directory=UPLOAD_DIR), name="uploads")


# ==================== API Endpoints ====================

@app.get("/")
async def root():
    """Health check endpoint."""
    return {
        "status": "online",
        "service": "ScareCrowWeb API",
        "version": "1.0.0"
    }


@app.post("/upload")
async def upload_image(
    image: UploadFile = File(...),
    device_id: str = Form(...),
    timestamp: Optional[str] = Form(None)
):
    """
    Receive image from ESP32-CAM device.
    
    - Saves image to uploads directory
    - Runs YOLO detection
    - Stores results in database
    - Creates alerts for animal detections
    """
    try:
        # Parse timestamp or use current time
        if timestamp:
            try:
                detection_time = datetime.fromisoformat(timestamp)
            except:
                detection_time = datetime.now()
        else:
            detection_time = datetime.now()
        
        # Generate unique filename
        file_ext = os.path.splitext(image.filename)[1] or ".jpg"
        unique_filename = f"{device_id}_{detection_time.strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex[:8]}{file_ext}"
        file_path = os.path.join(UPLOAD_DIR, unique_filename)
        
        # Save uploaded image
        with open(file_path, "wb") as buffer:
            shutil.copyfileobj(image.file, buffer)
        
        print(f"📷 Image received from {device_id}: {unique_filename}")
        
        # Update device status
        upsert_device(device_id)
        
        # Run YOLO detection
        detections, annotated_path = detect_animals(file_path, draw_boxes=True)
        animal_name, confidence = get_primary_detection(detections)
        
        # Use annotated image path if available
        image_path = os.path.basename(annotated_path) if annotated_path else unique_filename
        
        # Save detection to database
        detection_id = save_detection(
            device_id=device_id,
            animal=animal_name,
            confidence=confidence,
            image_path=image_path,
            timestamp=detection_time
        )
        
        # Create alert for animal detection
        if animal_name != 'none' and confidence > 0.5:
            alert_message = f"🐾 {animal_name.upper()} detected with {confidence:.0%} confidence"
            save_alert(device_id, alert_message)
        
        print(f"✅ Detection complete: {animal_name} ({confidence:.2%})")
        
        return JSONResponse({
            "success": True,
            "message": "Image processed successfully",
            "detection": {
                "id": detection_id,
                "device_id": device_id,
                "animal": animal_name,
                "confidence": confidence,
                "image_path": image_path,
                "timestamp": detection_time.isoformat(),
                "all_detections": detections
            }
        })
        
    except Exception as e:
        print(f"❌ Error processing image: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/detections")
async def get_detections(limit: int = 100):
    """Get all detection logs."""
    detections = get_all_detections(limit)
    return {
        "success": True,
        "count": len(detections),
        "detections": detections
    }


@app.get("/devices")
async def get_devices():
    """Get all device statuses."""
    devices = get_all_devices()
    return {
        "success": True,
        "count": len(devices),
        "devices": devices
    }


@app.get("/alerts")
async def get_alerts(limit: int = 100):
    """Get all alerts."""
    alerts = get_all_alerts(limit)
    return {
        "success": True,
        "count": len(alerts),
        "alerts": alerts
    }


@app.delete("/alerts/{alert_id}")
async def remove_alert(alert_id: int):
    """Delete an alert."""
    deleted = delete_alert(alert_id)
    if deleted:
        return {"success": True, "message": "Alert deleted"}
    raise HTTPException(status_code=404, detail="Alert not found")


@app.get("/stats")
async def get_stats():
    """Get dashboard statistics."""
    return {
        "success": True,
        "stats": {
            "total_detections": get_detection_count(),
            "active_devices": get_active_device_count(),
            "total_alerts": get_alert_count(),
            "recent_detections": get_recent_detections(5)
        }
    }


@app.get("/images/{filename}")
async def get_image(filename: str):
    """Serve an uploaded image."""
    file_path = os.path.join(UPLOAD_DIR, filename)
    if os.path.exists(file_path):
        return FileResponse(file_path)
    raise HTTPException(status_code=404, detail="Image not found")


# ==================== Device Configuration Endpoints ====================

@app.get("/devices/{device_id}/config")
async def get_config(device_id: str):
    """
    Get configuration for a specific device.
    Creates default config if device doesn't have one.
    ESP32 devices poll this endpoint to fetch their settings.
    """
    config = get_or_create_config(device_id)
    return {
        "success": True,
        "config": config
    }


@app.put("/devices/{device_id}/config")
async def update_config(device_id: str, config_data: dict):
    """
    Update configuration for a specific device.
    Only provided fields are updated, others remain unchanged.
    """
    try:
        updated_config = update_device_config(device_id, config_data)
        if updated_config:
            return {
                "success": True,
                "message": "Configuration updated",
                "config": updated_config
            }
        raise HTTPException(status_code=500, detail="Failed to update config")
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))


# ==================== Server Info Endpoint ====================

@app.get("/info")
async def get_server_info():
    """
    Get server information for device discovery.
    ESP32 devices can use this to verify connection and get server details.
    """
    return {
        "success": True,
        "server": {
            "name": "ScareCrowWeb",
            "version": "2.0.0",
            "hostname": f"{MDNS_HOSTNAME}.local",
            "port": MDNS_PORT,
            "ip": get_local_ip(),
            "mdns_enabled": MDNS_AVAILABLE,
            "endpoints": {
                "upload": "/upload",
                "config": "/devices/{device_id}/config",
                "status": "/",
                "docs": "/docs"
            }
        }
    }


# ==================== Run Server ====================

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000, reload=True)
