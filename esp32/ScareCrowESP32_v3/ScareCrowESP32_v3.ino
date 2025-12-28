/*
 * ScareCrow ESP32 Firmware v3.1
 * Simplified Version - No Captive Portal
 * 
 * Features:
 * - Direct WiFi connection (credentials in config.h)
 * - Analog sensor support (1V output)
 * - Status LED feedback
 * - Camera capture & upload
 * - Deterrent activation with configurable servo duration
 * 
 * WIRING:
 * - Analog Sensor: GPIO 2 (MUST use GPIO 1-3 for ADC with WiFi!)
 * - LED: GPIO 48
 * - Buzzer: GPIO 21
 * - Servo 1: GPIO 41
 * - Servo 2: GPIO 42
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include "esp_camera.h"
#include "config.h"

// Freenove ESP32-S3 CAM pin definitions
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

// ==================== GLOBALS ====================
Preferences prefs;
HTTPClient http;
Servo servo1, servo2;

// Configuration
String deviceId;
String serverUrl;

// Dynamic settings (can be changed from frontend)
unsigned long servoDuration = DEFAULT_SERVO_DURATION;  // Default 20 seconds

// State
bool wifiConnected = false;
unsigned long lastTrigger = 0;
unsigned long lastConfigFetch = 0;
int triggerCount = 0;

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  ScareCrow ESP32 Firmware v3.1");
    Serial.println("  (Simplified - No Captive Portal)");
    Serial.println("========================================\n");
    
    // Initialize LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    // Load saved config
    prefs.begin("scarecrow", false);
    loadConfig();
    
    // Initialize camera
    if (!initCamera()) {
        Serial.println("!! Camera init failed - continuing anyway");
    }
    
    // Initialize servos
    ESP32PWM::allocateTimer(0);
    servo1.setPeriodHertz(50);
    servo2.setPeriodHertz(50);
    servo1.attach(PIN_SERVO1, 500, 2400);
    servo2.attach(PIN_SERVO2, 500, 2400);
    servo1.write(90);
    servo2.write(90);
    
    // Buzzer
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // Connect to WiFi using hardcoded credentials
    connectWiFi();
    
    Serial.println("\n========================================");
    Serial.println("Setup complete!");
    Serial.printf("Sensor on GPIO %d, threshold %d\n", PIN_SENSOR, SENSOR_THRESHOLD);
    Serial.printf("Cooldown: %d seconds\n", COOLDOWN_SECONDS);
    Serial.printf("Servo duration: %lu ms\n", servoDuration);
    Serial.println("========================================\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected - reconnecting...");
        wifiConnected = false;
        connectWiFi();
    }
    
    // Read and log sensor
    static unsigned long lastLog = 0;
    int sensorValue = analogRead(PIN_SENSOR);
    
    if (millis() - lastLog > 3000) {
        Serial.printf("📊 Sensor: %d (trigger at %d)\n", sensorValue, SENSOR_THRESHOLD);
        lastLog = millis();
    }
    
    // Check for trigger
    if (sensorValue >= SENSOR_THRESHOLD) {
        unsigned long now = millis();
        if (now - lastTrigger > (COOLDOWN_SECONDS * 1000UL)) {
            Serial.println("🚨 TRIGGERED!");
            triggerCount++;
            lastTrigger = now;
            
            delay(CAPTURE_DELAY_MS);
            captureAndUpload();
            
            if (AUTO_DETERRENT) {
                activateDeterrent();
            }
        }
    }
    
    // Periodic config fetch from server
    if (wifiConnected && millis() - lastConfigFetch > CONFIG_FETCH_INTERVAL) {
        fetchConfig();
        lastConfigFetch = millis();
    }
    
    delay(100);
}

// ==================== WIFI ====================
void connectWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    blinkLED(3, 100);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\n✓ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        digitalWrite(PIN_LED, HIGH);
        delay(1000);
        digitalWrite(PIN_LED, LOW);
    } else {
        Serial.println("\n✗ WiFi connection failed!");
        Serial.println("Check WIFI_SSID and WIFI_PASSWORD in config.h");
        // Blink LED rapidly to indicate error
        blinkLED(20, 100);
    }
}

// ==================== CONFIG ====================
void loadConfig() {
    deviceId = prefs.getString("deviceId", "");
    serverUrl = prefs.getString("serverUrl", DEFAULT_SERVER_URL);
    servoDuration = prefs.getULong("servoDur", DEFAULT_SERVO_DURATION);
    
    // Generate device ID if empty
    if (deviceId.length() == 0) {
        uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
        deviceId = "SCARECROW-" + String(chipId, HEX);
        deviceId.toUpperCase();
        prefs.putString("deviceId", deviceId);
    }
    
    Serial.printf("Device: %s\n", deviceId.c_str());
    Serial.printf("Server: %s\n", serverUrl.c_str());
    Serial.printf("Servo Duration: %lu ms\n", servoDuration);
}

void saveConfig() {
    prefs.putString("serverUrl", serverUrl);
    prefs.putULong("servoDur", servoDuration);
    Serial.println("✓ Config saved");
}

void fetchConfig() {
    String url = serverUrl + "/devices/" + deviceId + "/config";
    
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    
    if (code == 200) {
        String payload = http.getString();
        
        // Parse JSON response
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc.containsKey("config")) {
            JsonObject config = doc["config"];
            
            // Update servo duration if provided (server sends seconds, we store milliseconds)
            if (config.containsKey("servo_duration")) {
                unsigned long durationSec = config["servo_duration"].as<unsigned long>();
                unsigned long newDuration = durationSec * 1000UL;  // Convert seconds to ms
                if (newDuration != servoDuration && durationSec >= 1 && durationSec <= 60) {
                    servoDuration = newDuration;
                    prefs.putULong("servoDur", servoDuration);
                    Serial.printf("✓ Servo duration updated: %lu ms (%lu sec)\n", servoDuration, durationSec);
                }
            }
            
            // Update server URL if provided
            if (config.containsKey("server_url")) {
                String newUrl = config["server_url"].as<String>();
                if (newUrl.length() > 0 && newUrl != serverUrl) {
                    serverUrl = newUrl;
                    prefs.putString("serverUrl", serverUrl);
                    Serial.printf("✓ Server URL updated: %s\n", serverUrl.c_str());
                }
            }
        }
        Serial.println("✓ Config fetched from server");
    } else if (code > 0) {
        Serial.printf("Config fetch failed: %d\n", code);
    } else {
        Serial.println("Config fetch failed: no connection");
    }
    http.end();
}

// ==================== CAMERA ====================
bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.frame_size = FRAMESIZE_VGA;
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }
    Serial.println("✓ Camera initialized");
    return true;
}

void captureAndUpload() {
    Serial.println("📷 Capturing...");
    blinkLED(2, 50);
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Capture failed");
        return;
    }
    
    Serial.printf("Image: %d bytes\n", fb->len);
    
    // Upload to server
    if (wifiConnected && serverUrl.length() > 0) {
        String url = serverUrl + "/upload/esp32";
        
        http.begin(url);
        http.setTimeout(10000);
        http.addHeader("Content-Type", "image/jpeg");
        http.addHeader("X-Device-ID", deviceId);
        
        int code = http.POST(fb->buf, fb->len);
        
        if (code == 200) {
            Serial.println("✓ Uploaded!");
        } else {
            Serial.printf("Upload failed: %d\n", code);
        }
        http.end();
    }
    
    esp_camera_fb_return(fb);
}

// ==================== DETERRENT ====================
void activateDeterrent() {
    Serial.printf("⚠️ Activating deterrent for %lu ms...\n", servoDuration);
    
    unsigned long startTime = millis();
    unsigned long endTime = startTime + servoDuration;
    
    // Wave servos for the configured duration
    while (millis() < endTime) {
        servo1.write(45);
        servo2.write(135);
        delay(300);
        
        if (millis() >= endTime) break;
        
        servo1.write(135);
        servo2.write(45);
        delay(300);
        
        // Beep every cycle
        digitalWrite(PIN_BUZZER, HIGH);
        delay(50);
        digitalWrite(PIN_BUZZER, LOW);
    }
    
    // Return servos to center
    servo1.write(90);
    servo2.write(90);
    digitalWrite(PIN_BUZZER, LOW);
    
    // Flash LED
    blinkLED(5, 50);
    
    Serial.println("✓ Deterrent complete");
}

// ==================== UTILITY ====================
void blinkLED(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(PIN_LED, HIGH);
        delay(duration);
        digitalWrite(PIN_LED, LOW);
        delay(duration);
    }
}
