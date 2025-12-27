# ScareCrowWeb Documentation

## Remote Configuration Architecture

This document describes how the ScareCrowWeb system enables remote configuration of ESP32-CAM devices through the web interface.

---

## System Overview

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│   Web Dashboard │ ──────► │  FastAPI Backend │ ──────► │    ESP32-CAM    │
│  (Configuration)│         │  (Store Settings)│         │ (Fetch & Apply) │
└─────────────────┘         └─────────────────┘         └─────────────────┘
                                    │
                                    ▼
                            ┌─────────────────┐
                            │    Database     │
                            │ (device_config) │
                            └─────────────────┘
```

### Configuration Sync Methods

| Method | How ESP32 Gets Config | Pros | Cons |
|--------|----------------------|------|------|
| **Polling** | ESP32 periodically calls `GET /devices/{id}/config` | Simple, works through NAT/firewalls | Slight delay (5-30 sec) |
| **WebSocket** | Real-time push from backend to ESP32 | Instant updates | More complex |

**Current Implementation:** Polling (30-second interval)

---

## Configurable Settings

### Servo Motor
| Setting | Description | Range | Default |
|---------|-------------|-------|---------|
| `servo_speed` | Rotation speed (degrees/sec) | 1-180 | 90 |
| `servo_angle` | Maximum sweep angle | 0-180 | 180 |
| `servo_duration` | Active duration (seconds) | 1-30 | 3 |

### PIR Motion Sensor
| Setting | Description | Range | Default |
|---------|-------------|-------|---------|
| `pir_delay_ms` | Trigger delay (milliseconds) | 0-5000 | 500 |
| `pir_cooldown_sec` | Cooldown between triggers (seconds) | 1-300 | 10 |

### LED Indicator
| Setting | Description | Options/Range | Default |
|---------|-------------|---------------|---------|
| `led_pattern` | Flash pattern | solid, blink, strobe, fade | blink |
| `led_speed_ms` | Blink interval (milliseconds) | 50-2000 | 200 |
| `led_duration_sec` | Active duration (seconds) | 1-60 | 5 |

### Sound Alarm
| Setting | Description | Options/Range | Default |
|---------|-------------|---------------|---------|
| `sound_type` | Alarm sound type | beep, siren, chirp | beep |
| `sound_volume` | Volume level | 0-100 | 80 |
| `sound_duration_sec` | Alarm duration (seconds) | 1-30 | 3 |

### General Settings
| Setting | Description | Type | Default |
|---------|-------------|------|---------|
| `auto_deterrent` | Automatically activate deterrent on detection | boolean | true |
| `detection_threshold` | Minimum confidence to trigger (0.0-1.0) | float | 0.5 |

---

## Database Schema

```sql
CREATE TABLE device_config (
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
    -- General
    auto_deterrent BOOLEAN DEFAULT 1,
    detection_threshold REAL DEFAULT 0.5,
    updated_at DATETIME,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

---

## API Endpoints

### GET /devices/{device_id}/config
Retrieve configuration for a specific device.

**Response:**
```json
{
  "success": true,
  "config": {
    "device_id": "ESP32-CAM-001",
    "servo_speed": 90,
    "servo_angle": 180,
    "servo_duration": 3,
    "pir_delay_ms": 500,
    "pir_cooldown_sec": 10,
    "led_pattern": "blink",
    "led_speed_ms": 200,
    "led_duration_sec": 5,
    "sound_type": "beep",
    "sound_volume": 80,
    "sound_duration_sec": 3,
    "auto_deterrent": true,
    "detection_threshold": 0.5,
    "updated_at": "2024-01-15T10:30:00"
  }
}
```

### PUT /devices/{device_id}/config
Update configuration for a specific device.

**Request Body:**
```json
{
  "servo_speed": 120,
  "led_pattern": "strobe",
  "auto_deterrent": true
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuration updated",
  "config": { ... }
}
```

---

## ESP32 Configuration Flow

```
┌──────────────────────────────────────────────────────────────┐
│                     ESP32 Boot Sequence                       │
├──────────────────────────────────────────────────────────────┤
│ 1. Connect to WiFi                                            │
│ 2. GET /devices/{device_id}/config  ◄── Fetch initial config │
│ 3. Apply settings to servo, LED, buzzer                       │
│ 4. Start PIR monitoring loop                                  │
│                                                               │
│ Every 30 seconds:                                             │
│   - GET /devices/{device_id}/config                           │
│   - If config changed → apply new settings                    │
└──────────────────────────────────────────────────────────────┘
```

---

## Hardware Wiring Reference

### ESP32-CAM Pin Assignments

| Component | GPIO Pin | Notes |
|-----------|----------|-------|
| PIR Sensor (OUT) | GPIO 13 | Digital input, HIGH on motion |
| **Servo 1** (Left Arm) | GPIO 12 | PWM output, sweeps LEFT on trigger |
| **Servo 2** (Right Arm) | GPIO 14 | PWM output, sweeps RIGHT (mirrored) |
| LED | GPIO 4 | Built-in flash LED |
| Buzzer (+) | GPIO 2 | PWM for tones |
| SIM800L TX | GPIO 15 | Optional cellular module |
| SIM800L RX | GPIO 16 | Optional cellular module |
| Camera | Built-in | OV2640 |

### Dual Servo Wiring (Mirrored Movement)

```
                    ┌─────────────────────┐
                    │     ESP32-CAM       │
                    │                     │
    PIR Sensor ─────┤ GPIO 13        3.3V ├───── PIR VCC
                    │                     │
  Servo 1 (L) ──────┤ GPIO 12         GND ├───── Common GND
                    │                     │
  Servo 2 (R) ──────┤ GPIO 14             │
                    │                     │
    (Built-in) ─────┤ GPIO 4 (Flash LED)  │
                    │                     │
    Buzzer (+) ─────┤ GPIO 2              │
                    │                     │
    SIM800L TX ─────┤ GPIO 15 (optional)  │
                    │                     │
    SIM800L RX ─────┤ GPIO 16 (optional)  │
                    └─────────────────────┘

Servo Movement Logic:
- Servo 1: 0° → 180° (Left arm sweeps left-to-right)
- Servo 2: 180° → 0° (Right arm sweeps right-to-left - MIRRORED)
- Creates synchronized "waving arms" effect
```

### Power Requirements
- ESP32-CAM: 5V, 2A recommended
- Servo x2: 5V (separate power recommended for high-torque)
- PIR: 3.3V-5V
- Buzzer: 3.3V
- SIM800L: 4.0V-4.4V (LiPo battery or buck converter)

---

## Connectivity Options

The device supports multiple connectivity modes:

### 1. WiFi Mode (Default for home/barn use)
- Connects to local WiFi network
- Lower power consumption
- Faster data transfer

### 2. Cellular Mode (SIM800L for field deployment)
- Uses 2G/GPRS via SIM800L module
- Works anywhere with cellular coverage
- Ideal for crops/remote areas
- Requires SIM card with data plan

### Connection Priority
```
┌─────────────────────────────────────────────────────────────┐
│                   ESP32 Boot Sequence                        │
├─────────────────────────────────────────────────────────────┤
│ 1. Check if WiFi credentials exist in flash                 │
│    ├─ YES → Try to connect to saved WiFi network            │
│    │        ├─ Connected? → Use WiFi mode                   │
│    │        └─ Failed? → Fall back to SIM800L              │
│    └─ NO → Start Captive Portal (AP Mode)                   │
│                                                              │
│ 2. Captive Portal Mode (Config Mode)                        │
│    - Creates hotspot: "ScareCrow-XXXX"                      │
│    - User connects to hotspot                               │
│    - Opens web page to configure WiFi/Cellular              │
│    - Saves settings to flash                                │
│    - Reboots and connects                                   │
└─────────────────────────────────────────────────────────────┘
```

---

## Captive Portal (Initial Setup)

When the device has no saved WiFi credentials or cannot connect:

### What Happens:
1. ESP32 creates its own hotspot: **"ScareCrow-XXXX"** (password: `scarecrow123`)
2. User connects phone/laptop to this hotspot
3. A configuration page auto-opens (or navigate to `192.168.4.1`)
4. User enters:
   - WiFi SSID and password
   - OR enables cellular mode (if SIM800L attached)
   - Device ID (auto-generated or custom)
   - Server URL (your backend address)
5. Device saves settings and reboots
6. Device connects to configured network

### Configuration Portal Features:
- **WiFi Setup**: Enter SSID and password
- **Cellular Setup**: APN settings for SIM card
- **Server Config**: Backend URL (default: `http://your-server:8000`)
- **Device ID**: Unique identifier for this scarecrow
- **Reset Button**: Hold for 10s to reset all settings

---

## Updated Database Schema

```sql
-- Additional field for servo 2
ALTER TABLE device_config ADD COLUMN servo2_enabled INTEGER DEFAULT 1;
ALTER TABLE device_config ADD COLUMN connection_type TEXT DEFAULT 'wifi';  -- 'wifi' or 'cellular'
ALTER TABLE device_config ADD COLUMN cellular_apn TEXT DEFAULT '';
```

---

## LED Flash Patterns

| Pattern | Description |
|---------|-------------|
| `solid` | Constant on for duration |
| `blink` | On/off at led_speed_ms interval |
| `strobe` | Rapid flash (50ms interval) |
| `fade` | Smooth fade in/out |

---

## Sound Types

| Type | Description |
|------|-------------|
| `beep` | Simple beep pattern |
| `siren` | Rising/falling tone |
| `chirp` | Bird-like chirp sound |

---

## Cloud Migration Notes

For cloud deployment:

1. **Database**: Replace SQLite with PostgreSQL
   - Update connection string in `database.py`
   
2. **WebSocket (optional)**: For real-time config push
   - Add WebSocket endpoint for ESP32 connections
   - Push config changes immediately

3. **Authentication**: Add API key validation
   - ESP32 sends `X-API-Key` header
   - Validate before returning config
