/*
 * ScareCrow Configuration File
 * 
 * Edit these values to customize your device
 * Pins are configured to avoid conflicts with Freenove ESP32-S3 N16R8 CAM
 * 
 * SAFE PINS (not used by camera): 2, 21, 38, 39, 40, 41, 42, 47, 48
 * 
 * WARNING: GPIO 2 has a glitch at boot - avoid for sensitive outputs!
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== SERVER CONFIGURATION ====================
// Default server URL - can be changed via captive portal
#define DEFAULT_SERVER_URL "http://192.168.1.100:8000"

// ==================== CAPTIVE PORTAL ====================
#define AP_PASSWORD "scarecrow123"

// ==================== PIN DEFINITIONS ====================
// IMPORTANT: Only using safe pins that don't conflict with camera!
// Camera uses: GPIO 4,5,6,7,8,9,10,11,12,13,15,16,17,18
// Safe pins: GPIO 2, 21, 38, 39, 40, 41, 42, 47, 48

// PIR Motion Sensor
#define PIN_PIR 47         // Safe pin

// Dual Servos (waving arms - mirrored movement)  
#define PIN_SERVO1 41      // Safe pin
#define PIN_SERVO2 42      // Safe pin

// LED indicator (Changed from GPIO 2 - it has boot glitch!)
#define PIN_LED 48         // RGB LED on Freenove board, no boot glitch

// Buzzer/Speaker
#define PIN_BUZZER 21      // Safe pin

// SIM800L (optional cellular module)
#define PIN_SIM800L_TX 40  // Safe pin
#define PIN_SIM800L_RX 39  // Safe pin (changed from 38)

// Spare safe pin: 38

// Reset button (hold 10s to reset config)
#define PIN_RESET_BUTTON 0 // Boot button

// ==================== TIMING DEFAULTS ====================
#define CONFIG_FETCH_INTERVAL 30000   // Fetch config every 30 seconds
#define WIFI_CONNECT_TIMEOUT 20000    // WiFi connection timeout (20s)
#define UPLOAD_TIMEOUT 15000          // Image upload timeout (15s)

// ==================== CAMERA DEFAULTS ====================
#define CAMERA_FRAME_SIZE FRAMESIZE_VGA    // 640x480
#define CAMERA_JPEG_QUALITY 12              // 0-63, lower = better quality

#endif // CONFIG_H
