# ScareCrow Firmware Binaries

This folder contains the pre-compiled firmware binaries for web-based ESP32 flashing.

## ⚠️ Binaries Not Included

The `.bin` files are not included in the repository because:
1. They are large binary files
2. They need to be compiled with your specific settings

## How to Compile Firmware

### Step 1: Open Arduino IDE

Open the sketch: `esp32/ScareCrowESP32/ScareCrowESP32.ino`

### Step 2: Select Board & Configure

**For ESP32-S3 Freenove/Eye:**
- Board: `ESP32S3 Dev Module`
- USB Mode: `USB-OTG (TinyUSB)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`

**For ESP32-CAM:**
- Board: `AI Thinker ESP32-CAM`
- Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`

### Step 3: Edit camera_pins.h

Uncomment the correct board definition:
```cpp
#define CAMERA_MODEL_ESP32S3_FREENOVE   // For Freenove
// or
#define CAMERA_MODEL_AI_THINKER         // For ESP32-CAM
```

### Step 4: Export Compiled Binary

In Arduino IDE:
- Go to `Sketch > Export Compiled Binary`
- Wait for compilation to complete
- Find the `.bin` files in the sketch folder

### Step 5: Copy Binaries Here

For ESP32-S3, you need:
- `esp32s3-bootloader.bin` - from `.arduino/packages/.../bootloader.bin`
- `esp32s3-partitions.bin` - from exported partition file
- `esp32s3-firmware.bin` - from exported `.ino.bin` file

For ESP32-CAM, you need:
- `esp32cam-bootloader.bin`
- `esp32cam-partitions.bin`  
- `esp32cam-firmware.bin`

## Binary Offsets

| Board | Bootloader | Partitions | Firmware |
|-------|-----------|------------|----------|
| ESP32-S3 | 0x0 | 0x8000 | 0x10000 |
| ESP32 | 0x1000 | 0x8000 | 0x10000 |

## Alternative: Get Pre-built Binaries

If you have PlatformIO installed, you can build with:

```bash
cd esp32/ScareCrowESP32
pio run -e esp32s3
# Binaries will be in .pio/build/esp32s3/
```

## File Structure

```
firmware/
├── manifest-esp32s3.json     # Manifest for ESP32-S3 boards
├── manifest-esp32cam.json    # Manifest for ESP32-CAM
├── manifest-esp32.json       # Manifest for generic ESP32
├── README.md                 # This file
│
├── esp32s3-bootloader.bin    # (you need to add)
├── esp32s3-partitions.bin    # (you need to add)
├── esp32s3-firmware.bin      # (you need to add)
│
├── esp32cam-bootloader.bin   # (you need to add)
├── esp32cam-partitions.bin   # (you need to add)
└── esp32cam-firmware.bin     # (you need to add)
```

## Testing Web Flash

After adding binaries:
1. Open the ScareCrowWeb dashboard
2. Go to "Add Device" page
3. Connect your ESP32 via USB
4. Select your board type
5. Click "Flash ScareCrow Firmware"

**Note:** Web Serial requires Chrome, Edge, or Opera browser.
