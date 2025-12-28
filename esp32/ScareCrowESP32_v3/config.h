/*
 * ScareCrow ESP32 Configuration v3.1
 * 
 * IMPORTANT PIN NOTES FOR ESP32-S3 CAM:
 * - Camera uses: GPIO 4,5,6,7,8,9,10,11,12,13,15,16,17,18
 * - ADC1 pins (work with WiFi): GPIO 1,2,3 (GPIO 4-10 used by camera)
 * - Safe digital pins: GPIO 21, 38, 39, 40, 41, 42, 47, 48
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== WIFI CREDENTIALS ====================
// IMPORTANT: Set your WiFi credentials here before flashing!
#define WIFI_SSID "YOUR_WIFI_SSID"       // <-- CHANGE THIS
#define WIFI_PASSWORD "YOUR_WIFI_PASS"   // <-- CHANGE THIS

// ==================== SERVER ====================
#define DEFAULT_SERVER_URL "http://192.168.0.103:8000"  // <-- Change to your server

// ==================== SENSOR ====================
// Sensor connected to GPIO 2 (ADC1 - works with WiFi)
// For 1V analog sensor that outputs for 20 seconds
#define PIN_SENSOR 2
#define SENSOR_THRESHOLD 400    // ADC value to trigger (0-4095)
#define COOLDOWN_SECONDS 60     // Wait between triggers (default 1 min)

// ==================== HARDWARE ====================
#define PIN_LED 48              // Status LED
#define PIN_BUZZER 21           // Buzzer/alarm
#define PIN_SERVO1 41           // Servo 1
#define PIN_SERVO2 42           // Servo 2

// ==================== DETERRENT BEHAVIOR ====================
#define AUTO_DETERRENT true             // Activate deterrent on detection
#define DEFAULT_SERVO_DURATION 20000    // Servo activation time in ms (20 seconds default)
#define CAPTURE_DELAY_MS 500            // Delay before photo after trigger
#define CONFIG_FETCH_INTERVAL 30000     // Fetch config every 30s

// Note: Servo duration can be changed from frontend via /devices/{id}/config endpoint

#endif
