/*
 * ScareCrowWeb ESP32 Firmware
 * 
 * Main Arduino sketch for the ScareCrow animal detection/deterrent system
 * 
 * Supported Boards:
 * - ESP32-S3 WROOM Freenove (dual USB-C)
 * - ESP32-CAM AI-Thinker
 * - ESP32-S3-EYE
 * 
 * Features:
 * - OV2640 Camera for image capture
 * - Dual servo control (mirrored waving arms)
 * - PIR motion sensor trigger
 * - LED flash patterns
 * - Buzzer/speaker alarm
 * - WiFi + Captive Portal for configuration
 * - Optional SIM800L cellular connectivity
 * - Remote configuration from web dashboard
 * 
 * Libraries Required (install via Arduino Library Manager):
 * - WiFiManager by tzapu
 * - ArduinoJson by Benoit Blanchon
 * - ESP32Servo by Kevin Harrington
 * - TinyGSM by Volodymyr Shymanskyy (optional, for SIM800L)
 * 
 * Board Setup:
 * - Board: ESP32S3 Dev Module (or your specific board)
 * - PSRAM: OPI PSRAM
 * - USB Mode: USB-OTG (for Freenove dual USB-C)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

// NOTE: Brownout detector disable code removed - incompatible with Arduino-ESP32 v3.x
// If you experience brownout issues, use a better power supply (5V 2A+)

// Include board-specific configurations
#include "camera_pins.h"
#include "config.h"

// ==================== Global Objects ====================
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
HTTPClient http;

Servo servo1;
Servo servo2;

// ==================== Configuration (loaded from preferences) ====================
struct DeviceConfig {
    char deviceId[32];
    char serverUrl[128];
    char wifiSSID[64];
    char wifiPassword[64];
    bool useCellular;
    char cellularAPN[64];
    
    // Actuator settings (fetched from server)
    int servoSpeed;
    int servoAngle;
    int servoDuration;
    int pirDelayMs;
    int pirCooldownSec;
    String ledPattern;
    int ledSpeedMs;
    int ledDurationSec;
    String soundType;
    int soundVolume;
    int soundDurationSec;
    bool autoDeterrent;
    float detectionThreshold;
};

DeviceConfig config;

// ==================== State Variables ====================
bool isConfigMode = false;
bool isConnected = false;
unsigned long lastConfigFetch = 0;
unsigned long lastMotionDetected = 0;
unsigned long configFetchInterval = 30000;  // 30 seconds

// ==================== Forward Declarations ====================
void setupCamera();
void setupActuators();
void startCaptivePortal();
void handleRoot();
void handleSave();
void handleStatus();
bool connectToWiFi();
void fetchRemoteConfig();
void captureAndUpload();
void uploadImage(uint8_t* imageData, size_t imageLen);
void activateDeterrent();
void waveServos();
void flashLED();
void playAlarm();

// ==================== SETUP ====================
void setup() {
    // Note: Brownout detector disable removed (incompatible with Arduino-ESP32 v3.x)
    // If experiencing power issues, use a quality 5V 2A+ power supply
    
    Serial.begin(115200);
    delay(2000);  // Wait for serial and power to stabilize
    
    Serial.println("\n========================================");
    Serial.println("   ScareCrowWeb ESP32 Firmware v1.0");
    Serial.println("========================================");
    Serial.println("Initializing...");
    
    // Initialize preferences
    preferences.begin("scarecrow", false);
    loadConfig();
    
    Serial.println("Power stable. Initializing hardware...");
    delay(500);
    
    // Setup actuators first (lower power draw)
    setupActuators();
    delay(1000);  // Let actuators settle
    
    // Long delay before camera to let power fully stabilize
    Serial.println("Preparing camera initialization...");
    delay(2000);
    
    // Setup camera (high power draw - do this last)
    setupCamera();
    delay(1000);  // Let camera settle
    
    // Try to connect to WiFi
    if (strlen(config.wifiSSID) > 0) {
        Serial.printf("Attempting to connect to WiFi: %s\n", config.wifiSSID);
        if (connectToWiFi()) {
            isConnected = true;
            Serial.println("✓ Connected to WiFi!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            
            // Fetch initial config from server
            fetchRemoteConfig();
        } else {
            Serial.println("✗ Failed to connect to WiFi");
            startCaptivePortal();
        }
    } else {
        Serial.println("No WiFi credentials saved");
        startCaptivePortal();
    }
    
    Serial.println("========================================");
    Serial.println("Setup complete! Monitoring for motion...");
    Serial.println("========================================\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Handle captive portal if in config mode
    if (isConfigMode) {
        dnsServer.processNextRequest();
        server.handleClient();
        return;
    }
    
    // Check PIR sensor for motion
    if (digitalRead(PIN_PIR) == HIGH) {
        unsigned long currentTime = millis();
        
        // Check cooldown period
        if (currentTime - lastMotionDetected > (config.pirCooldownSec * 1000)) {
            Serial.println("🚨 Motion detected!");
            
            // Apply delay before capture
            delay(config.pirDelayMs);
            
            // Capture and upload image
            captureAndUpload();
            
            // Activate deterrent if enabled
            if (config.autoDeterrent) {
                activateDeterrent();
            }
            
            lastMotionDetected = currentTime;
        }
    }
    
    // Periodically fetch configuration from server
    if (millis() - lastConfigFetch > configFetchInterval) {
        fetchRemoteConfig();
        lastConfigFetch = millis();
    }
    
    delay(100);  // Small delay to prevent CPU hogging
}

// ==================== CAMERA SETUP ====================
void setupCamera() {
    Serial.println("Initializing camera with conservative settings...");
    
    camera_config_t cameraConfig;
    // IMPORTANT: Use LEDC timer 3 and channel 7 to avoid conflicts with servos
    // Servos use timers 0-1 and channels 0-3
    cameraConfig.ledc_channel = LEDC_CHANNEL_7;
    cameraConfig.ledc_timer = LEDC_TIMER_3;
    cameraConfig.pin_d0 = Y2_GPIO_NUM;
    cameraConfig.pin_d1 = Y3_GPIO_NUM;
    cameraConfig.pin_d2 = Y4_GPIO_NUM;
    cameraConfig.pin_d3 = Y5_GPIO_NUM;
    cameraConfig.pin_d4 = Y6_GPIO_NUM;
    cameraConfig.pin_d5 = Y7_GPIO_NUM;
    cameraConfig.pin_d6 = Y8_GPIO_NUM;
    cameraConfig.pin_d7 = Y9_GPIO_NUM;
    cameraConfig.pin_xclk = XCLK_GPIO_NUM;
    cameraConfig.pin_pclk = PCLK_GPIO_NUM;
    cameraConfig.pin_vsync = VSYNC_GPIO_NUM;
    cameraConfig.pin_href = HREF_GPIO_NUM;
    cameraConfig.pin_sccb_sda = SIOD_GPIO_NUM;
    cameraConfig.pin_sccb_scl = SIOC_GPIO_NUM;
    cameraConfig.pin_pwdn = PWDN_GPIO_NUM;
    cameraConfig.pin_reset = RESET_GPIO_NUM;
    
    // Use lower clock frequency to reduce power draw
    cameraConfig.xclk_freq_hz = 10000000;  // 10MHz instead of 20MHz
    
    // Start with lower resolution (less power)
    cameraConfig.frame_size = FRAMESIZE_QVGA;  // 320x240 initially (lower power)
    cameraConfig.pixel_format = PIXFORMAT_JPEG;
    cameraConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    cameraConfig.fb_location = CAMERA_FB_IN_PSRAM;
    cameraConfig.jpeg_quality = 15;  // Slightly lower quality to reduce power
    cameraConfig.fb_count = 1;       // Single buffer to reduce memory/power
    
    // Initialize with conservative settings first
    esp_err_t err = esp_camera_init(&cameraConfig);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        Serial.println("Will continue without camera...");
        return;
    }
    
    Serial.println("✓ Camera initialized with conservative settings");
    
    // After successful init, we can upgrade settings if PSRAM available
    delay(500);  // Let camera stabilize
    
    if (psramFound()) {
        Serial.println("PSRAM found. Upgrading camera settings...");
        sensor_t * s = esp_camera_sensor_get();
        if (s) {
            // Gradually increase settings
            s->set_framesize(s, FRAMESIZE_VGA);  // Upgrade to 640x480
            delay(100);
        }
        Serial.println("✓ Camera upgraded to VGA resolution");
    }
    
    Serial.println("✓ Camera ready!");
}

// ==================== ACTUATOR SETUP ====================
void setupActuators() {
    Serial.println("Initializing actuators...");
    
    // Setup PIR sensor
    pinMode(PIN_PIR, INPUT);
    
    // Setup LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    // Setup buzzer
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // Setup servos - use timers 0 and 1 (camera uses timer 3)
    // ESP32-S3 has 8 LEDC channels (0-7), all low-speed mode
    ESP32PWM::allocateTimer(0);  // Timer for servo 1
    ESP32PWM::allocateTimer(1);  // Timer for servo 2
    
    servo1.setPeriodHertz(50);
    servo2.setPeriodHertz(50);
    
    // Attach servos to specific channels to avoid camera conflict
    // Camera uses channel 7, so we use channels 0 and 1
    servo1.attach(PIN_SERVO1, 500, 2400);  // Will use channel 0
    servo2.attach(PIN_SERVO2, 500, 2400);  // Will use channel 1
    
    // Center servos
    servo1.write(90);
    servo2.write(90);
    
    Serial.println("✓ Actuators initialized");
}

// ==================== CONFIG MANAGEMENT ====================
void loadConfig() {
    Serial.println("Loading configuration...");
    
    // Load from preferences (non-volatile storage)
    String deviceId = preferences.getString("deviceId", "");
    String serverUrl = preferences.getString("serverUrl", DEFAULT_SERVER_URL);
    String wifiSSID = preferences.getString("wifiSSID", "");
    String wifiPassword = preferences.getString("wifiPassword", "");
    bool useCellular = preferences.getBool("useCellular", false);
    String cellularAPN = preferences.getString("cellularAPN", "");
    
    // Copy to config struct
    strncpy(config.deviceId, deviceId.c_str(), sizeof(config.deviceId) - 1);
    strncpy(config.serverUrl, serverUrl.c_str(), sizeof(config.serverUrl) - 1);
    strncpy(config.wifiSSID, wifiSSID.c_str(), sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, wifiPassword.c_str(), sizeof(config.wifiPassword) - 1);
    config.useCellular = useCellular;
    strncpy(config.cellularAPN, cellularAPN.c_str(), sizeof(config.cellularAPN) - 1);
    
    // Generate device ID if not set
    if (strlen(config.deviceId) == 0) {
        uint64_t chipId = ESP.getEfuseMac();
        snprintf(config.deviceId, sizeof(config.deviceId), "SCARECROW-%04X", (uint16_t)(chipId & 0xFFFF));
    }
    
    // Set defaults for actuator config
    config.servoSpeed = 90;
    config.servoAngle = 180;
    config.servoDuration = 3;
    config.pirDelayMs = 500;
    config.pirCooldownSec = 10;
    config.ledPattern = "blink";
    config.ledSpeedMs = 200;
    config.ledDurationSec = 5;
    config.soundType = "beep";
    config.soundVolume = 80;
    config.soundDurationSec = 3;
    config.autoDeterrent = true;
    config.detectionThreshold = 0.5;
    
    Serial.printf("Device ID: %s\n", config.deviceId);
    Serial.printf("Server URL: %s\n", config.serverUrl);
}

void saveConfig() {
    preferences.putString("deviceId", config.deviceId);
    preferences.putString("serverUrl", config.serverUrl);
    preferences.putString("wifiSSID", config.wifiSSID);
    preferences.putString("wifiPassword", config.wifiPassword);
    preferences.putBool("useCellular", config.useCellular);
    preferences.putString("cellularAPN", config.cellularAPN);
    Serial.println("✓ Configuration saved");
}

// ==================== WIFI CONNECTION ====================
bool connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    Serial.printf("  SSID: '%s'\n", config.wifiSSID);
    Serial.printf("  Password length: %d\n", strlen(config.wifiPassword));
    
    // Disconnect first
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSSID, config.wifiPassword);
    
    // Try for 30 seconds (60 attempts * 500ms)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        // Show status every 10 attempts
        if (attempts % 10 == 0) {
            Serial.printf(" (status: %d)\n", WiFi.status());
        }
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✓ WiFi connected!");
        Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        Serial.printf("✗ WiFi failed. Status: %d\n", WiFi.status());
        Serial.println("  (3=disconnected, 4=failed, 6=wrong password)");
        return false;
    }
}

// ==================== CAPTIVE PORTAL ====================
void startCaptivePortal() {
    Serial.println("Starting Captive Portal...");
    isConfigMode = true;
    
    // Create AP name
    String apName = String("ScareCrow-") + String(config.deviceId).substring(10);
    
    // Disconnect any previous WiFi
    WiFi.disconnect(true);
    delay(100);
    
    // Set WiFi mode to AP
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configure AP with specific IP
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);
    delay(100);
    
    // Start AP
    bool apStarted = WiFi.softAP(apName.c_str(), AP_PASSWORD);
    delay(500);  // Wait for AP to fully start
    
    if (apStarted) {
        Serial.println("✓ Access Point started");
    } else {
        Serial.println("✗ Failed to start Access Point");
        return;
    }
    
    Serial.printf("AP SSID: %s\n", apName.c_str());
    Serial.printf("AP Password: %s\n", AP_PASSWORD);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // Setup DNS server to redirect all domains
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.println("✓ DNS Server started");
    
    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/generate_204", HTTP_GET, handleRoot);  // Android captive portal
    server.on("/fwlink", HTTP_GET, handleRoot);        // Microsoft captive portal
    server.onNotFound(handleRoot);
    
    server.begin();
    Serial.println("✓ Web Server started on port 80");
    Serial.println("✓ Captive Portal ready!");
    Serial.println("Connect to WiFi and go to: http://192.168.4.1");
}

void handleRoot() {
    Serial.println("Serving config page...");
    
    // Use a simple, minimal HTML to reduce memory usage
    String html = F("<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ScareCrow</title>"
        "<style>"
        "body{font-family:Arial;background:#222;color:#fff;padding:20px;max-width:400px;margin:auto}"
        "h2{color:#4f4;text-align:center}"
        "input,select{width:100%;padding:8px;margin:5px 0 15px;border:1px solid #444;background:#333;color:#fff;box-sizing:border-box}"
        "button{width:100%;padding:12px;background:#4f4;border:none;color:#000;font-weight:bold;cursor:pointer}"
        "</style></head><body>"
        "<h2>ScareCrow Setup</h2>");
    
    html += "<p style='text-align:center;color:#888'>Device: " + String(config.deviceId) + "</p>";
    html += F("<form method='POST' action='/save'>"
        "<label>WiFi SSID:</label><input name='ssid' value='");
    html += String(config.wifiSSID);
    html += F("' required>"
        "<label>WiFi Password:</label><input type='password' name='password' value='");
    html += String(config.wifiPassword);
    html += F("'>"
        "<label>Server URL:</label><input name='server' value='");
    html += String(config.serverUrl);
    html += F("' placeholder='http://IP:8000'>"
        "<button type='submit'>Save & Connect</button>"
        "</form></body></html>");
    
    server.send(200, "text/html", html);
    Serial.println("Page sent!");
}

void handleSave() {
    if (server.hasArg("ssid")) {
        strncpy(config.wifiSSID, server.arg("ssid").c_str(), sizeof(config.wifiSSID) - 1);
    }
    if (server.hasArg("password")) {
        strncpy(config.wifiPassword, server.arg("password").c_str(), sizeof(config.wifiPassword) - 1);
    }
    if (server.hasArg("server")) {
        strncpy(config.serverUrl, server.arg("server").c_str(), sizeof(config.serverUrl) - 1);
    }
    if (server.hasArg("connection")) {
        config.useCellular = (server.arg("connection") == "cellular");
    }
    if (server.hasArg("apn")) {
        strncpy(config.cellularAPN, server.arg("apn").c_str(), sizeof(config.cellularAPN) - 1);
    }
    
    saveConfig();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Saved!</title>
    <style>
        body { background: #1a1a2e; color: white; font-family: sans-serif; text-align: center; padding: 40px; }
        .success { font-size: 64px; }
        h1 { margin-top: 20px; }
        p { color: #94a3b8; }
    </style>
</head>
<body>
    <div class="success">✅</div>
    <h1>Settings Saved!</h1>
    <p>Device will now restart and connect to your network...</p>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
    
    delay(2000);
    ESP.restart();
}

void handleStatus() {
    String json = "{\"deviceId\":\"" + String(config.deviceId) + "\",\"connected\":" + (isConnected ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

// ==================== REMOTE CONFIG FETCH ====================
void fetchRemoteConfig() {
    if (!isConnected) return;
    
    String url = String(config.serverUrl) + "/devices/" + String(config.deviceId) + "/config";
    Serial.printf("Fetching config from: %s\n", url.c_str());
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        
        // Parse JSON
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc["success"]) {
            JsonObject cfg = doc["config"];
            
            config.servoSpeed = cfg["servo_speed"] | 90;
            config.servoAngle = cfg["servo_angle"] | 180;
            config.servoDuration = cfg["servo_duration"] | 3;
            config.pirDelayMs = cfg["pir_delay_ms"] | 500;
            config.pirCooldownSec = cfg["pir_cooldown_sec"] | 10;
            config.ledPattern = cfg["led_pattern"] | "blink";
            config.ledSpeedMs = cfg["led_speed_ms"] | 200;
            config.ledDurationSec = cfg["led_duration_sec"] | 5;
            config.soundType = cfg["sound_type"] | "beep";
            config.soundVolume = cfg["sound_volume"] | 80;
            config.soundDurationSec = cfg["sound_duration_sec"] | 3;
            config.autoDeterrent = cfg["auto_deterrent"] | true;
            config.detectionThreshold = cfg["detection_threshold"] | 0.5;
            
            Serial.println("✓ Remote config updated");
        }
    } else {
        Serial.printf("Config fetch failed, HTTP code: %d\n", httpCode);
    }
    
    http.end();
}

// ==================== IMAGE CAPTURE & UPLOAD ====================
void captureAndUpload() {
    Serial.println("📷 Capturing image...");
    
    // Turn on LED flash
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    
    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        digitalWrite(PIN_LED, LOW);
        return;
    }
    
    digitalWrite(PIN_LED, LOW);
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    // Upload to server
    if (isConnected) {
        uploadImage(fb->buf, fb->len);
    }
    
    esp_camera_fb_return(fb);
}

void uploadImage(uint8_t* imageData, size_t imageLen) {
    String url = String(config.serverUrl) + "/upload";
    Serial.printf("Uploading to: %s\n", url.c_str());
    
    // Create multipart form data
    String boundary = "----ScareCrowBoundary" + String(millis());
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
    bodyStart += String(config.deviceId) + "\r\n";
    bodyStart += "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    bodyStart += "Content-Type: image/jpeg\r\n\r\n";
    String bodyEnd = "\r\n--" + boundary + "--\r\n";
    
    int contentLength = bodyStart.length() + imageLen + bodyEnd.length();
    
    WiFiClient client;
    if (!client.connect(config.serverUrl, 8000)) {
        Serial.println("Connection to server failed");
        return;
    }
    
    // Parse host from URL
    String host = String(config.serverUrl);
    host.replace("http://", "");
    host.replace("https://", "");
    int portIndex = host.indexOf(':');
    if (portIndex > 0) {
        host = host.substring(0, portIndex);
    }
    
    // Send HTTP request
    client.println("POST /upload HTTP/1.1");
    client.println("Host: " + host);
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(contentLength));
    client.println("Connection: close");
    client.println();
    
    // Send body start
    client.print(bodyStart);
    
    // Send image data in chunks
    int chunkSize = 1024;
    for (int i = 0; i < imageLen; i += chunkSize) {
        int remaining = imageLen - i;
        int toSend = (remaining < chunkSize) ? remaining : chunkSize;
        client.write(&imageData[i], toSend);
    }
    
    // Send body end
    client.print(bodyEnd);
    
    // Wait for response
    unsigned long timeout = millis() + 10000;
    while (client.available() == 0) {
        if (millis() > timeout) {
            Serial.println("Upload timeout");
            client.stop();
            return;
        }
        delay(100);
    }
    
    // Read response
    while (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
    }
    
    client.stop();
    Serial.println("✓ Image uploaded successfully");
}

// ==================== DETERRENT ACTIONS ====================
void activateDeterrent() {
    Serial.println("🚨 Activating deterrent...");
    
    // Run all deterrent actions in parallel using tasks or sequentially
    waveServos();
    flashLED();
    playAlarm();
    
    Serial.println("✓ Deterrent sequence complete");
}

void waveServos() {
    Serial.println("Waving servos...");
    
    unsigned long startTime = millis();
    unsigned long duration = config.servoDuration * 1000;
    
    while (millis() - startTime < duration) {
        // Servo 1: 0 to angle
        // Servo 2: angle to 0 (mirrored)
        for (int pos = 0; pos <= config.servoAngle; pos += 5) {
            servo1.write(pos);
            servo2.write(config.servoAngle - pos);  // Mirrored
            delay(1000 / config.servoSpeed);
            
            if (millis() - startTime >= duration) break;
        }
        
        // Return
        for (int pos = config.servoAngle; pos >= 0; pos -= 5) {
            servo1.write(pos);
            servo2.write(config.servoAngle - pos);  // Mirrored
            delay(1000 / config.servoSpeed);
            
            if (millis() - startTime >= duration) break;
        }
    }
    
    // Return to center
    servo1.write(90);
    servo2.write(90);
}

void flashLED() {
    Serial.printf("Flashing LED: %s pattern\n", config.ledPattern.c_str());
    
    unsigned long startTime = millis();
    unsigned long duration = config.ledDurationSec * 1000;
    
    if (config.ledPattern == "solid") {
        digitalWrite(PIN_LED, HIGH);
        delay(duration);
        digitalWrite(PIN_LED, LOW);
    } 
    else if (config.ledPattern == "blink") {
        while (millis() - startTime < duration) {
            digitalWrite(PIN_LED, HIGH);
            delay(config.ledSpeedMs);
            digitalWrite(PIN_LED, LOW);
            delay(config.ledSpeedMs);
        }
    }
    else if (config.ledPattern == "strobe") {
        while (millis() - startTime < duration) {
            digitalWrite(PIN_LED, HIGH);
            delay(50);
            digitalWrite(PIN_LED, LOW);
            delay(50);
        }
    }
    else if (config.ledPattern == "fade") {
        // PWM fade (requires LED on PWM pin)
        while (millis() - startTime < duration) {
            for (int i = 0; i < 255; i += 5) {
                analogWrite(PIN_LED, i);
                delay(10);
            }
            for (int i = 255; i > 0; i -= 5) {
                analogWrite(PIN_LED, i);
                delay(10);
            }
        }
    }
    
    digitalWrite(PIN_LED, LOW);
}

void playAlarm() {
    Serial.printf("Playing alarm: %s\n", config.soundType.c_str());
    
    unsigned long startTime = millis();
    unsigned long duration = config.soundDurationSec * 1000;
    
    if (config.soundType == "beep") {
        while (millis() - startTime < duration) {
            tone(PIN_BUZZER, 1000, 200);
            delay(300);
        }
    }
    else if (config.soundType == "siren") {
        while (millis() - startTime < duration) {
            // Rising tone
            for (int freq = 500; freq < 1500; freq += 50) {
                tone(PIN_BUZZER, freq, 20);
                delay(20);
            }
            // Falling tone
            for (int freq = 1500; freq > 500; freq -= 50) {
                tone(PIN_BUZZER, freq, 20);
                delay(20);
            }
        }
    }
    else if (config.soundType == "chirp") {
        while (millis() - startTime < duration) {
            // Bird-like chirp
            tone(PIN_BUZZER, 2000, 50);
            delay(60);
            tone(PIN_BUZZER, 2500, 50);
            delay(60);
            tone(PIN_BUZZER, 2200, 50);
            delay(200);
        }
    }
    
    noTone(PIN_BUZZER);
}
