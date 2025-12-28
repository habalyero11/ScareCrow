# ScareCrow ESP32 Firmware v3.1

Simplified ESP32-CAM firmware for the ScareCrow animal detection system.

## Changes in v3.1
- **Removed captive portal** - WiFi credentials are now hardcoded in `config.h`
- **20-second servo duration** by default (was 2 seconds)
- **Servo duration configurable from frontend** - Settings → Configuration page

## Quick Setup

1. **Edit `config.h`** with your WiFi credentials:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   #define DEFAULT_SERVER_URL "http://your-server-ip:8000"
   ```

2. **Flash the ESP32** using Arduino IDE or PlatformIO

3. **Done!** The device will:
   - Connect to your WiFi automatically
   - Upload images to your backend server
   - Wave servos for 20 seconds when triggered
   - Fetch config updates from server every 30 seconds

## Configurable Settings

The following can be changed from the frontend (Configuration page):
- **Servo Duration** (1-60 seconds)
- **Detection Threshold**
- **Auto Deterrent** (on/off)

## Hardware Wiring

| Component | GPIO Pin |
|-----------|----------|
| Analog Sensor | GPIO 2 |
| LED | GPIO 48 |
| Buzzer | GPIO 21 |
| Servo 1 | GPIO 41 |
| Servo 2 | GPIO 42 |

## Troubleshooting

**WiFi won't connect:**
- Check WIFI_SSID and WIFI_PASSWORD in config.h
- LED will blink rapidly if connection fails
- Serial monitor shows connection status

**Servo not moving:**
- Check servo power supply (5V, sufficient current)
- Verify GPIO connections (41, 42)
