/*
 * ScareCrow Configuration File v2.0
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
// If mDNS is enabled, device will try "scarecrow.local" first
#define DEFAULT_SERVER_URL "http://192.168.1.100:8000"
#define DEFAULT_SERVER_PORT 8000

// ==================== mDNS CONFIGURATION ====================
// mDNS allows using friendly names instead of IP addresses
// Your server will be accessible as "http://scarecrow.local:8000"
#define MDNS_ENABLED true
#define MDNS_SERVER_NAME "scarecrow"      // Server hostname: scarecrow.local
#define MDNS_DEVICE_PREFIX "scarecrow-"   // Device will be: scarecrow-XXXX.local

// ==================== CAPTIVE PORTAL ====================
#define AP_PASSWORD "scarecrow123"

// ==================== PIN DEFINITIONS ====================
// IMPORTANT: Only using safe pins that don't conflict with camera!
// Camera uses: GPIO 4,5,6,7,8,9,10,11,12,13,15,16,17,18
// Safe pins: GPIO 2, 21, 38, 39, 40, 41, 42, 47, 48

// ==================== SENSOR CONFIGURATION ====================
// SENSOR_TYPE options:
//   0 = PIR digital sensor (outputs HIGH when triggered)
//   1 = Analog sensor (outputs voltage when triggered, e.g. 1V for 20 seconds)
#define DEFAULT_SENSOR_TYPE 1        // Using analog sensor

// Motion Sensor Pin
// IMPORTANT: For ANALOG sensors, you MUST use ADC1 pins (GPIO 1-10) on ESP32-S3!
// ADC2 pins (GPIO 11-20) don't work when WiFi is active.
// Camera uses: GPIO 4,5,6,7,8,9,10 - so GPIO 1, 2, 3 are available for ADC
// We use GPIO 2 which is safe and supports ADC1
#define PIN_SENSOR 2               // GPIO 2 - ADC1 capable, safe pin

// For DIGITAL/PIR sensors, you can use any GPIO (like 47)
// If using PIR, you can change to: #define PIN_SENSOR 47

// Analog sensor threshold (for 1V sensor on 3.3V reference)
// ESP32-S3 ADC is 12-bit (0-4095), default attenuation reads up to ~2.5V
// 1V input = approximately 1640 raw value (1V / 2.5V * 4095)
// Using 500 as threshold to account for noise and give margin
#define ANALOG_SENSOR_THRESHOLD 500  

// Trigger cooldown - how long to wait between triggers
// This prevents multiple triggers from a single detection event
#define DEFAULT_COOLDOWN_SEC 60      // Default 1 minute (60 seconds)
#define MIN_COOLDOWN_SEC 60          // Minimum 1 minute
#define MAX_COOLDOWN_SEC 600         // Maximum 10 minutes

// Dual Servos (waving arms - mirrored movement)  
#define PIN_SERVO1 41      // Safe pin
#define PIN_SERVO2 42      // Safe pin

// LED indicator 
#define PIN_LED 48         // RGB LED on Freenove board, no boot glitch

// Buzzer/Speaker
#define PIN_BUZZER 21      // Safe pin

// SIM800L (optional cellular module)
#define PIN_SIM800L_TX 40  // Safe pin
#define PIN_SIM800L_RX 39  // Safe pin (changed from 38)

// Spare safe pin: 38

// Reset button (hold to reset config)
#define PIN_RESET_BUTTON 0 // Boot button

// ==================== STATUS LED PATTERNS ====================
// LED blink patterns for visual feedback (no serial monitor needed)
// Pattern: milliseconds on, milliseconds off
#define LED_PATTERN_PORTAL_ON   1000    // Slow blink: captive portal mode
#define LED_PATTERN_PORTAL_OFF  1000
#define LED_PATTERN_CONNECT_ON  200     // Fast blink: connecting to WiFi
#define LED_PATTERN_CONNECT_OFF 200
#define LED_PATTERN_SUCCESS_ON  2000    // Long on: connected successfully
#define LED_PATTERN_ERROR_ON    100     // Rapid blink: error
#define LED_PATTERN_ERROR_OFF   100
#define LED_PATTERN_ERROR_COUNT 3       // Number of error blinks
#define LED_PATTERN_UPLOAD_ON   50      // Quick flash: uploading
#define LED_PATTERN_UPLOAD_OFF  50

// ==================== BUTTON CONFIG ====================
#define RESET_BUTTON_HOLD_TIME 5000     // Hold 5 seconds to reset config

// ==================== TIMING DEFAULTS ====================
#define CONFIG_FETCH_INTERVAL 30000   // Fetch config every 30 seconds
#define WIFI_CONNECT_TIMEOUT 20000    // WiFi connection timeout (20s)
#define UPLOAD_TIMEOUT 15000          // Image upload timeout (15s)

// ==================== CAMERA DEFAULTS ====================
#define CAMERA_FRAME_SIZE FRAMESIZE_VGA    // 640x480
#define CAMERA_JPEG_QUALITY 12              // 0-63, lower = better quality

#endif // CONFIG_H
