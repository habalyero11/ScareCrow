"""
SQLite database setup and operations.

CLOUD MIGRATION POINT:
- Replace sqlite3 with asyncpg for PostgreSQL
- Update connection string to use environment variable
- Update SQL syntax if needed (SQLite -> PostgreSQL differences)
"""
import sqlite3
import os
from datetime import datetime
from typing import List, Optional, Dict, Any

# Database file path - CLOUD MIGRATION: Replace with PostgreSQL connection string
DATABASE_PATH = os.path.join(os.path.dirname(__file__), "scarecrow.db")


def get_connection():
    """
    Get database connection.
    
    CLOUD MIGRATION POINT: Replace with:
    - asyncpg.connect(os.environ['DATABASE_URL'])
    - Or use SQLAlchemy with async support
    """
    conn = sqlite3.connect(DATABASE_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_database():
    """
    Initialize database tables.
    Called automatically on application startup.
    """
    conn = get_connection()
    cursor = conn.cursor()
    
    # Create devices table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS devices (
            device_id TEXT PRIMARY KEY,
            last_seen DATETIME,
            status TEXT DEFAULT 'offline'
        )
    """)
    
    # Create detections table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS detections (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            animal TEXT,
            confidence REAL,
            image_path TEXT,
            timestamp DATETIME,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        )
    """)
    
    # Create alerts table
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            message TEXT,
            timestamp DATETIME,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        )
    """)
    
    # Create device_config table for remote ESP32 configuration
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS device_config (
            device_id TEXT PRIMARY KEY,
            -- Servo settings
            servo_speed INTEGER DEFAULT 90,
            servo_angle INTEGER DEFAULT 180,
            servo_duration INTEGER DEFAULT 3,
            -- PIR sensor settings
            pir_delay_ms INTEGER DEFAULT 500,
            pir_cooldown_sec INTEGER DEFAULT 10,
            -- LED settings
            led_pattern TEXT DEFAULT 'blink',
            led_speed_ms INTEGER DEFAULT 200,
            led_duration_sec INTEGER DEFAULT 5,
            -- Sound settings
            sound_type TEXT DEFAULT 'beep',
            sound_volume INTEGER DEFAULT 80,
            sound_duration_sec INTEGER DEFAULT 3,
            -- General settings
            auto_deterrent INTEGER DEFAULT 1,
            detection_threshold REAL DEFAULT 0.5,
            updated_at DATETIME,
            FOREIGN KEY (device_id) REFERENCES devices(device_id)
        )
    """)
    
    conn.commit()
    conn.close()
    print("✅ Database initialized successfully")


# ==================== Device Operations ====================

def upsert_device(device_id: str) -> None:
    """Insert or update device record."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        INSERT INTO devices (device_id, last_seen, status)
        VALUES (?, ?, 'online')
        ON CONFLICT(device_id) DO UPDATE SET
            last_seen = excluded.last_seen,
            status = 'online'
    """, (device_id, datetime.now()))
    
    conn.commit()
    conn.close()


def get_all_devices() -> List[Dict[str, Any]]:
    """Get all devices with their status."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT device_id, last_seen, status FROM devices
        ORDER BY last_seen DESC
    """)
    
    devices = [dict(row) for row in cursor.fetchall()]
    conn.close()
    return devices


# ==================== Detection Operations ====================

def save_detection(device_id: str, animal: str, confidence: float, 
                   image_path: str, timestamp: datetime) -> int:
    """Save detection result to database."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        INSERT INTO detections (device_id, animal, confidence, image_path, timestamp)
        VALUES (?, ?, ?, ?, ?)
    """, (device_id, animal, confidence, image_path, timestamp))
    
    detection_id = cursor.lastrowid
    conn.commit()
    conn.close()
    return detection_id


def get_all_detections(limit: int = 100) -> List[Dict[str, Any]]:
    """Get all detections, most recent first."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT id, device_id, animal, confidence, image_path, timestamp
        FROM detections
        ORDER BY timestamp DESC
        LIMIT ?
    """, (limit,))
    
    detections = [dict(row) for row in cursor.fetchall()]
    conn.close()
    return detections


def get_recent_detections(limit: int = 5) -> List[Dict[str, Any]]:
    """Get most recent detections for dashboard."""
    return get_all_detections(limit)


def get_detection_count() -> int:
    """Get total detection count."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("SELECT COUNT(*) FROM detections")
    count = cursor.fetchone()[0]
    conn.close()
    return count


# ==================== Alert Operations ====================

def save_alert(device_id: str, message: str) -> int:
    """Save alert to database."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        INSERT INTO alerts (device_id, message, timestamp)
        VALUES (?, ?, ?)
    """, (device_id, message, datetime.now()))
    
    alert_id = cursor.lastrowid
    conn.commit()
    conn.close()
    return alert_id


def get_all_alerts(limit: int = 100) -> List[Dict[str, Any]]:
    """Get all alerts, most recent first."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT id, device_id, message, timestamp
        FROM alerts
        ORDER BY timestamp DESC
        LIMIT ?
    """, (limit,))
    
    alerts = [dict(row) for row in cursor.fetchall()]
    conn.close()
    return alerts


def get_alert_count() -> int:
    """Get total alert count."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("SELECT COUNT(*) FROM alerts")
    count = cursor.fetchone()[0]
    conn.close()
    return count


def delete_alert(alert_id: int) -> bool:
    """Delete an alert by ID."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("DELETE FROM alerts WHERE id = ?", (alert_id,))
    deleted = cursor.rowcount > 0
    conn.commit()
    conn.close()
    return deleted


# ==================== Statistics ====================

def get_active_device_count() -> int:
    """Get count of devices that were online in the last 5 minutes."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT COUNT(*) FROM devices 
        WHERE status = 'online'
    """)
    
    count = cursor.fetchone()[0]
    conn.close()
    return count


# ==================== Device Configuration Operations ====================

# Default configuration values
DEFAULT_CONFIG = {
    'servo_speed': 90,
    'servo_angle': 180,
    'servo_duration': 3,
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
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT device_id, servo_speed, servo_angle, servo_duration,
               pir_delay_ms, pir_cooldown_sec,
               led_pattern, led_speed_ms, led_duration_sec,
               sound_type, sound_volume, sound_duration_sec,
               auto_deterrent, detection_threshold, updated_at
        FROM device_config
        WHERE device_id = ?
    """, (device_id,))
    
    row = cursor.fetchone()
    conn.close()
    
    if row:
        config = dict(row)
        # Convert SQLite integer to Python bool
        config['auto_deterrent'] = bool(config['auto_deterrent'])
        return config
    return None


def create_default_config(device_id: str) -> Dict[str, Any]:
    """Create default configuration for a device."""
    conn = get_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        INSERT INTO device_config (device_id, updated_at)
        VALUES (?, ?)
    """, (device_id, datetime.now()))
    
    conn.commit()
    conn.close()
    
    # Return the default config with device_id
    config = {'device_id': device_id, **DEFAULT_CONFIG, 'updated_at': datetime.now().isoformat()}
    return config


def update_device_config(device_id: str, config_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Update configuration for a specific device."""
    conn = get_connection()
    cursor = conn.cursor()
    
    # Check if config exists, create if not
    cursor.execute("SELECT 1 FROM device_config WHERE device_id = ?", (device_id,))
    if not cursor.fetchone():
        # Create default config first
        cursor.execute("""
            INSERT INTO device_config (device_id, updated_at)
            VALUES (?, ?)
        """, (device_id, datetime.now()))
    
    # Build dynamic update query based on provided fields
    valid_fields = [
        'servo_speed', 'servo_angle', 'servo_duration',
        'pir_delay_ms', 'pir_cooldown_sec',
        'led_pattern', 'led_speed_ms', 'led_duration_sec',
        'sound_type', 'sound_volume', 'sound_duration_sec',
        'auto_deterrent', 'detection_threshold'
    ]
    
    updates = []
    values = []
    
    for field in valid_fields:
        if field in config_data:
            updates.append(f"{field} = ?")
            value = config_data[field]
            # Convert bool to int for SQLite
            if isinstance(value, bool):
                value = 1 if value else 0
            values.append(value)
    
    if updates:
        updates.append("updated_at = ?")
        values.append(datetime.now())
        values.append(device_id)
        
        query = f"UPDATE device_config SET {', '.join(updates)} WHERE device_id = ?"
        cursor.execute(query, values)
    
    conn.commit()
    conn.close()
    
    # Return updated config
    return get_device_config(device_id)


def get_or_create_config(device_id: str) -> Dict[str, Any]:
    """Get device config, creating default if doesn't exist."""
    config = get_device_config(device_id)
    if config is None:
        config = create_default_config(device_id)
    return config

