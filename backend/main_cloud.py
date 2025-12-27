"""
ScareCrowWeb Backend - FastAPI Application (Cloud Version)

Cloud-ready backend using Supabase for database and storage.
Designed for deployment on Google Cloud Run.

To run locally:
    uvicorn main_cloud:app --reload --host 0.0.0.0 --port 8000

To switch to cloud mode, update imports in main.py or rename this file.
"""
from fastapi import FastAPI, File, UploadFile, Form, HTTPException, Request, Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, RedirectResponse
from contextlib import asynccontextmanager
from datetime import datetime
from typing import Optional
import os
import uuid

from dotenv import load_dotenv
load_dotenv()

# Import Supabase modules
from database_supabase import (
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
from storage import upload_image, get_image_url, init_storage
from detection import detect_animals, get_primary_detection, load_model


# Temporary local uploads directory (for processing before upload to Supabase)
UPLOAD_DIR = "/tmp/uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)

# Server port (Cloud Run sets PORT env var)
PORT = int(os.environ.get('PORT', 8000))


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan events."""
    # Startup
    print("🚀 Starting ScareCrowWeb Backend (Cloud Mode)...")
    init_database()  # Verify Supabase connection
    init_storage()   # Verify storage bucket
    load_model()     # Pre-load YOLO model
    
    print("✅ Backend ready!")
    print(f"🌐 Server running on port {PORT}")
    
    yield
    
    # Shutdown
    print("👋 Shutting down ScareCrowWeb Backend...")


app = FastAPI(
    title="ScareCrowWeb API",
    description="Backend API for ESP32-CAM animal detection system (Cloud Version)",
    version="2.0.0",
    lifespan=lifespan
)

# CORS Configuration - Allow frontend access from anywhere
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allow all origins for now
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ==================== API Endpoints ====================

@app.get("/")
async def root():
    """Health check endpoint."""
    return {
        "status": "online",
        "service": "ScareCrowWeb API",
        "version": "2.0.0",
        "mode": "cloud"
    }


@app.post("/upload")
async def upload_image_endpoint(
    image: UploadFile = File(...),
    device_id: str = Form(...),
    timestamp: Optional[str] = Form(None)
):
    """
    Receive image from ESP32-CAM device.
    
    - Saves image to Supabase Storage
    - Runs YOLO detection
    - Stores results in Supabase database
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
        
        # Read image data
        image_data = await image.read()
        
        # Save temporarily for YOLO processing
        temp_path = os.path.join(UPLOAD_DIR, unique_filename)
        with open(temp_path, "wb") as f:
            f.write(image_data)
        
        print(f"📷 Image received from {device_id}: {unique_filename}")
        
        # Update device status
        upsert_device(device_id)
        
        # Run YOLO detection
        detections, annotated_path = detect_animals(temp_path, draw_boxes=True)
        animal_name, confidence = get_primary_detection(detections)
        
        # Upload annotated image to Supabase Storage
        if annotated_path and os.path.exists(annotated_path):
            with open(annotated_path, "rb") as f:
                annotated_data = f.read()
            annotated_filename = f"annotated_{unique_filename}"
            image_url = upload_image(annotated_data, annotated_filename)
            # Clean up local annotated file
            os.remove(annotated_path)
        else:
            # Upload original image
            image_url = upload_image(image_data, unique_filename)
            annotated_filename = unique_filename
        
        # Clean up temp file
        if os.path.exists(temp_path):
            os.remove(temp_path)
        
        # Save detection to database (store URL as image_path)
        detection_id = save_detection(
            device_id=device_id,
            animal=animal_name,
            confidence=confidence,
            image_path=image_url or annotated_filename,
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
                "image_path": image_url or annotated_filename,
                "timestamp": detection_time.isoformat(),
                "all_detections": detections
            }
        })
        
    except Exception as e:
        print(f"❌ Error processing image: {str(e)}")
        import traceback
        traceback.print_exc()
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/upload/esp32")
async def upload_esp32(
    request: Request,
    x_device_id: str = Header(None, alias="X-Device-ID")
):
    """
    Alternative upload endpoint for ESP32 devices.
    
    ESP32 sends raw JPEG with X-Device-ID header.
    This endpoint handles that format directly.
    """
    try:
        # Get device ID from header
        device_id = x_device_id
        if not device_id:
            raise HTTPException(status_code=400, detail="X-Device-ID header required")
        
        detection_time = datetime.now()
        
        # Read raw body (JPEG data)
        image_data = await request.body()
        if len(image_data) == 0:
            raise HTTPException(status_code=400, detail="No image data received")
        
        # Generate unique filename
        unique_filename = f"{device_id}_{detection_time.strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex[:8]}.jpg"
        
        # Save temporarily for YOLO processing
        temp_path = os.path.join(UPLOAD_DIR, unique_filename)
        with open(temp_path, "wb") as f:
            f.write(image_data)
        
        print(f"📷 ESP32 image received from {device_id}: {unique_filename} ({len(image_data)} bytes)")
        
        # Update device status
        upsert_device(device_id)
        
        # Run YOLO detection
        detections, annotated_path = detect_animals(temp_path, draw_boxes=True)
        animal_name, confidence = get_primary_detection(detections)
        
        # Upload annotated image to Supabase Storage
        if annotated_path and os.path.exists(annotated_path):
            with open(annotated_path, "rb") as f:
                annotated_data = f.read()
            annotated_filename = f"annotated_{unique_filename}"
            image_url = upload_image(annotated_data, annotated_filename)
            os.remove(annotated_path)
        else:
            image_url = upload_image(image_data, unique_filename)
            annotated_filename = unique_filename
        
        # Clean up temp file
        if os.path.exists(temp_path):
            os.remove(temp_path)
        
        # Save detection to database
        detection_id = save_detection(
            device_id=device_id,
            animal=animal_name,
            confidence=confidence,
            image_path=image_url or annotated_filename,
            timestamp=detection_time
        )
        
        # Create alert for animal detection
        if animal_name != 'none' and confidence > 0.5:
            alert_message = f"🐾 {animal_name.upper()} detected with {confidence:.0%} confidence"
            save_alert(device_id, alert_message)
        
        print(f"✅ ESP32 detection complete: {animal_name} ({confidence:.2%})")
        
        return JSONResponse({
            "success": True,
            "message": "Image processed successfully",
            "detection": {
                "id": detection_id,
                "device_id": device_id,
                "animal": animal_name,
                "confidence": confidence
            }
        })
        
    except HTTPException:
        raise
    except Exception as e:
        print(f"❌ Error processing ESP32 image: {str(e)}")
        import traceback
        traceback.print_exc()
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
    """Redirect to Supabase Storage URL for the image."""
    image_url = get_image_url(filename)
    return RedirectResponse(url=image_url)


# ==================== Device Configuration Endpoints ====================

@app.get("/devices/{device_id}/config")
async def get_config(device_id: str):
    """
    Get configuration for a specific device.
    Creates default config if device doesn't have one.
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
    """
    return {
        "success": True,
        "server": {
            "name": "ScareCrowWeb",
            "version": "2.0.0",
            "mode": "cloud",
            "port": PORT,
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
    uvicorn.run(app, host="0.0.0.0", port=PORT)
