# ScareCrow ESP32 Firmware

Arduino firmware for the ScareCrow animal detection and deterrent system.

## Supported Boards

| Board | Status | Notes |
|-------|--------|-------|
| **ESP32-S3 WROOM Freenove** | ✅ Default | Dual USB-C, OV2640 camera |
| ESP32-S3-EYE | ✅ Supported | Same pinout as Freenove |
| ESP32-CAM AI-Thinker | ✅ Supported | Classic ESP32-CAM |
| ESP-WROVER-KIT | ✅ Supported | Development kit |

## Features

- 📷 **Camera capture** on motion detection
- 🤖 **Dual servo control** (mirrored waving arms)
- 📡 **WiFi + Captive Portal** for easy configuration
- 📱 **Optional SIM800L** cellular connectivity
- ⚙️ **Remote configuration** from web dashboard
- 💡 **LED flash patterns** (solid, blink, strobe, fade)
- 🔊 **Sound alarms** (beep, siren, chirp)

## Prerequisites

### Arduino IDE Setup

1. **Install Arduino IDE 2.x** from [arduino.cc](https://www.arduino.cc/en/software)

2. **Add ESP32 Board Support**:
   - Go to `File > Preferences`
   - Add this URL to "Additional Boards Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to `Tools > Board > Boards Manager`
   - Search "ESP32" and install **"esp32 by Espressif Systems"**

3. **Install Required Libraries** (Sketch > Include Library > Manage Libraries):
   - `ArduinoJson` by Benoit Blanchon
   - `ESP32Servo` by Kevin Harrington
   - `WiFiManager` by tzapu (optional, for advanced portal)
   - `TinyGSM` by Volodymyr Shymanskyy (optional, for SIM800L)

### Board Configuration

For **ESP32-S3 WROOM Freenove**:
- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- USB Mode: `USB-OTG (TinyUSB)`
- PSRAM: `OPI PSRAM`
- Flash Size: `16MB (128Mb)` or your board's flash size
- Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`

## Wiring Diagram

```
ESP32-S3 Freenove Connections:

                 ┌────────────────────┐
                 │   ESP32-S3 WROOM   │
                 │                    │
 PIR Sensor ─────┤ GPIO 13            │
                 │                    │
 Servo 1 (L) ────┤ GPIO 12            │
                 │                    │
 Servo 2 (R) ────┤ GPIO 14            │
                 │                    │
 LED (Flash) ────┤ GPIO 4             │
                 │                    │
 Buzzer ─────────┤ GPIO 2             │
                 │                    │
 SIM800L TX ─────┤ GPIO 15 (optional) │
                 │                    │
 SIM800L RX ─────┤ GPIO 16 (optional) │
                 └────────────────────┘

Power:
- ESP32: 5V via USB-C
- Servos: 5V (external power recommended)
- PIR: 3.3V-5V
- Buzzer: 3.3V
- SIM800L: 4.0-4.4V (LiPo or buck converter)
```

## Quick Start

### 1. Open the Project
Open `ScareCrowESP32.ino` in Arduino IDE

### 2. Select Your Board
Edit `camera_pins.h`:
```cpp
// Uncomment your board:
#define CAMERA_MODEL_ESP32S3_FREENOVE   // <-- For Freenove
// #define CAMERA_MODEL_AI_THINKER      // <-- For classic ESP32-CAM
```

### 3. Configure Pins (optional)
Edit `config.h` if your wiring differs from defaults

### 4. Upload
- Connect ESP32 via USB
- Select correct port in `Tools > Port`
- Click Upload

### 5. Initial Setup
1. On first boot, ESP32 creates WiFi hotspot: **"ScareCrow-XXXX"**
2. Connect your phone to this hotspot
3. Password: `scarecrow123`
4. Browser opens automatically (or go to `192.168.4.1`)
5. Enter your WiFi credentials and server URL
6. Device restarts and connects

## Configuration

### Via Captive Portal (first time)
Connect to the device's hotspot and configure via web interface

### Via Web Dashboard (remote)
After device is connected, use the ScareCrowWeb dashboard:
1. Go to Configuration page
2. Select your device
3. Adjust servo, LED, sound settings
4. Click Save

Device polls for new config every 30 seconds.

## Dual Servo Movement

The scarecrow has two arms that move in opposite directions:

```
Servo 1 (Left):  0° ─────────────────▶ 180°
Servo 2 (Right): 180° ◀───────────────── 0°

Result: Both arms "wave" outward simultaneously
```

## File Structure

```
ScareCrowESP32/
├── ScareCrowESP32.ino   # Main sketch
├── config.h              # Pin definitions & settings
├── camera_pins.h         # Camera pin mappings for different boards
└── README.md             # This file
```

## Troubleshooting

### Camera not initializing
- Check board selection in `camera_pins.h`
- Ensure PSRAM is enabled in board settings
- Try reducing JPEG quality in `config.h`

### Can't connect to WiFi
- Verify credentials in captive portal
- Check signal strength
- Reset config: Hold BOOT button for 10 seconds

### Upload fails
- Try pressing BOOT button while uploading
- Check USB cable (some are charge-only)
- For Freenove: Use the correct USB-C port (typically the one labeled "USB")

### Servos not moving
- Check power supply (servos need 5V, >1A)
- Verify pin connections
- Test with a simple servo sweep sketch first

## API Endpoints Used

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/upload` | POST | Send captured image |
| `/devices/{id}/config` | GET | Fetch configuration |

## License

MIT License - Feel free to modify and use in your projects!
