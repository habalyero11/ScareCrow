# ScareCrow ESP32 Firmware v3.0

A clean, reliable firmware for the ESP32-S3 CAM (Freenove) with captive portal WiFi setup and 1V analog sensor support.

## Wiring

| Component | GPIO | Notes |
|-----------|------|-------|
| **Analog Sensor** | **GPIO 2** | MUST use GPIO 1-3 for ADC with WiFi! |
| LED | GPIO 48 | Status/deterrent LED |
| Buzzer | GPIO 21 | Alarm buzzer |
| Servo 1 | GPIO 41 | Arm wave |
| Servo 2 | GPIO 42 | Arm wave |

## First-Time Setup

1. Upload this firmware to ESP32
2. Connect to WiFi: `ScareCrow-XXXX` (password: `scarecrow123`)
3. Open `http://192.168.4.1` in your browser
4. Select your home WiFi, enter password
5. Enter your computer's IP running the backend (e.g., `http://192.168.0.103:8000`)
6. Click "Save & Connect"

## Finding Your Computer's IP

Run this command:
- **Windows**: `ipconfig` → Look for IPv4 Address
- **Mac/Linux**: `ifconfig` or `ip addr`

Make sure the ESP32 and your computer are on the same network!

## LED Status

- **Slow blink**: Captive portal mode (waiting for WiFi setup)
- **Fast blink**: Processing (camera capture, upload)
- **Solid**: Connected successfully

## Sensor Notes

Your 1V sensor that outputs for 20 seconds:
- Connect the output wire to **GPIO 2**
- Default threshold: 400 (out of 4095)
- Cooldown: 60 seconds between triggers

## Troubleshooting

**Can't see ScareCrow WiFi?**
- Press RST button on ESP32
- Wait 10-15 seconds
- Refresh WiFi list on your phone/computer

**Sensor reads 0?**
- Make sure wire is on **GPIO 2** (not 47!)
- GPIO 2 is the ONLY available ADC pin that works with WiFi

**Can't connect to server?**
- Check that ESP32 IP (192.168.X.X) and server IP are on same subnet
- Run the backend: `py -3.11 -m uvicorn main:app --host 0.0.0.0 --port 8000`
