"""
Supabase database operations for ScareCrowWeb.

This module replaces the SQLite database.py with Supabase PostgreSQL.
All function signatures remain the same for compatibility.
"""
import os
from datetime import datetime
from typing import List, Optional, Dict, Any
from dotenv import load_dotenv
from supabase import create_client, Client

# Load environment variables
load_dotenv()

# Supabase client (initialized lazily)
SUPABASE_URL = os.environ.get('SUPABASE_URL')
SUPABASE_KEY = os.environ.get('SUPABASE_KEY')

supabase: Client = None

def _get_client() -> Client:
    """Get or create Supabase client."""
    global supabase
    if supabase is None:
        if not SUPABASE_URL or not SUPABASE_KEY:
            print(f"⚠️ SUPABASE_URL or SUPABASE_KEY not set!")
            print(f"   SUPABASE_URL: {'set' if SUPABASE_URL else 'NOT SET'}")
            print(f"   SUPABASE_KEY: {'set' if SUPABASE_KEY else 'NOT SET'}")
            raise ValueError("SUPABASE_URL and SUPABASE_KEY environment variables are required")
        supabase = create_client(SUPABASE_URL, SUPABASE_KEY)
    return supabase


def init_database():
    """
    Initialize database connection.
    Tables should be created via Supabase SQL Editor.
    This function just verifies the connection.
    """
    try:
        client = _get_client()
        # Simple connection test
        result = client.table('devices').select('device_id').limit(1).execute()
        print("✅ Supabase database connected successfully")
    except Exception as e:
        print(f"⚠️ Supabase connection warning: {e}")
        print("   Make sure tables are created in Supabase SQL Editor")


# ==================== Device Operations ====================

def upsert_device(device_id: str) -> None:
    """Insert or update device record."""
    try:
        _get_client().table('devices').upsert({
            'device_id': device_id,
            'last_seen': datetime.now().isoformat(),
            'status': 'online'
        }).execute()
    except Exception as e:
        print(f"Error upserting device: {e}")


def get_all_devices() -> List[Dict[str, Any]]:
    """Get all devices with their status."""
    try:
        result = _get_client().table('devices').select('*').order('last_seen', desc=True).execute()
        return result.data
    except Exception as e:
        print(f"Error getting devices: {e}")
        return []


# ==================== Detection Operations ====================

def save_detection(device_id: str, animal: str, confidence: float, 
                   image_path: str, timestamp: datetime) -> int:
    """Save detection result to database."""
    try:
        result = _get_client().table('detections').insert({
            'device_id': device_id,
            'animal': animal,
            'confidence': confidence,
            'image_path': image_path,
            'timestamp': timestamp.isoformat()
        }).execute()
        return result.data[0]['id'] if result.data else 0
    except Exception as e:
        print(f"Error saving detection: {e}")
        return 0


def get_all_detections(limit: int = 100) -> List[Dict[str, Any]]:
    """Get all detections, most recent first."""
    try:
        result = _get_client().table('detections').select('*').order('timestamp', desc=True).limit(limit).execute()
        return result.data
    except Exception as e:
        print(f"Error getting detections: {e}")
        return []


def get_recent_detections(limit: int = 5) -> List[Dict[str, Any]]:
    """Get most recent detections for dashboard."""
    return get_all_detections(limit)


def get_detection_count() -> int:
    """Get total detection count."""
    try:
        result = _get_client().table('detections').select('id', count='exact').execute()
        return result.count or 0
    except Exception as e:
        print(f"Error getting detection count: {e}")
        return 0


# ==================== Alert Operations ====================

def save_alert(device_id: str, message: str) -> int:
    """Save alert to database."""
    try:
        result = _get_client().table('alerts').insert({
            'device_id': device_id,
            'message': message,
            'timestamp': datetime.now().isoformat()
        }).execute()
        return result.data[0]['id'] if result.data else 0
    except Exception as e:
        print(f"Error saving alert: {e}")
        return 0


def get_all_alerts(limit: int = 100) -> List[Dict[str, Any]]:
    """Get all alerts, most recent first."""
    try:
        result = _get_client().table('alerts').select('*').order('timestamp', desc=True).limit(limit).execute()
        return result.data
    except Exception as e:
        print(f"Error getting alerts: {e}")
        return []


def get_alert_count() -> int:
    """Get total alert count."""
    try:
        result = _get_client().table('alerts').select('id', count='exact').execute()
        return result.count or 0
    except Exception as e:
        print(f"Error getting alert count: {e}")
        return 0


def delete_alert(alert_id: int) -> bool:
    """Delete an alert by ID."""
    try:
        result = _get_client().table('alerts').delete().eq('id', alert_id).execute()
        return len(result.data) > 0
    except Exception as e:
        print(f"Error deleting alert: {e}")
        return False


# ==================== Statistics ====================

def get_active_device_count() -> int:
    """Get count of devices that are online."""
    try:
        result = _get_client().table('devices').select('device_id', count='exact').eq('status', 'online').execute()
        return result.count or 0
    except Exception as e:
        print(f"Error getting active device count: {e}")
        return 0


# ==================== Device Configuration Operations ====================

# Default configuration values
DEFAULT_CONFIG = {
    'servo_speed': 90,
    'servo_angle': 180,
    'servo_duration': 20,
    'pir_delay_ms': 500,
    'pir_cooldown_sec': 10,
    'led_pattern': 'blink',
    'led_speed_ms': 200,
    'led_duration_sec': 5,
    'sound_type': 'beep',
    'sound_volume': 80,
    'sound_duration_sec': 3,
    'auto_deterrent': True,
    'detection_threshold': 0.5
}


def get_device_config(device_id: str) -> Optional[Dict[str, Any]]:
    """Get configuration for a specific device."""
    try:
        result = _get_client().table('device_config').select('*').eq('device_id', device_id).execute()
        if result.data:
            return result.data[0]
        return None
    except Exception as e:
        print(f"Error getting device config: {e}")
        return None


def create_default_config(device_id: str) -> Dict[str, Any]:
    """Create default configuration for a device."""
    try:
        config_data = {
            'device_id': device_id,
            **DEFAULT_CONFIG,
            'updated_at': datetime.now().isoformat()
        }
        result = _get_client().table('device_config').insert(config_data).execute()
        if result.data:
            return result.data[0]
        return config_data
    except Exception as e:
        print(f"Error creating default config: {e}")
        return {'device_id': device_id, **DEFAULT_CONFIG}


def update_device_config(device_id: str, config_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Update configuration for a specific device."""
    try:
        # Check if config exists
        existing = get_device_config(device_id)
        if not existing:
            create_default_config(device_id)
        
        # Filter to only valid fields
        valid_fields = [
            'servo_speed', 'servo_angle', 'servo_duration',
            'pir_delay_ms', 'pir_cooldown_sec',
            'led_pattern', 'led_speed_ms', 'led_duration_sec',
            'sound_type', 'sound_volume', 'sound_duration_sec',
            'auto_deterrent', 'detection_threshold'
        ]
        
        update_data = {k: v for k, v in config_data.items() if k in valid_fields}
        update_data['updated_at'] = datetime.now().isoformat()
        
        result = _get_client().table('device_config').update(update_data).eq('device_id', device_id).execute()
        
        if result.data:
            return result.data[0]
        return get_device_config(device_id)
    except Exception as e:
        print(f"Error updating device config: {e}")
        return None


def get_or_create_config(device_id: str) -> Dict[str, Any]:
    """Get device config, creating default if doesn't exist."""
    config = get_device_config(device_id)
    if config is None:
        config = create_default_config(device_id)
    return config
