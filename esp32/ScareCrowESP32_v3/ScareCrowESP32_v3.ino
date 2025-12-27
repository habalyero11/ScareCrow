/*
 * ScareCrow ESP32 Firmware v3.0
 * Clean, Reliable Version
 * 
 * Features:
 * - WiFi captive portal for easy setup
 * - Analog sensor support (1V output)
 * - Status LED feedback
 * - Camera capture & upload
 * - Deterrent activation (servo, buzzer, LED)
 * 
 * WIRING:
 * - Analog Sensor: GPIO 2 (MUST use GPIO 1-3 for ADC with WiFi!)
 * - LED: GPIO 48
 * - Buzzer: GPIO 21
 * - Servo 1: GPIO 41
 * - Servo 2: GPIO 42
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
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
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;
HTTPClient http;
Servo servo1, servo2;

// Configuration
String deviceId;
String serverUrl;
String wifiSSID;
String wifiPass;

// State
bool inPortalMode = false;
bool wifiConnected = false;
unsigned long lastTrigger = 0;
unsigned long lastConfigFetch = 0;
int triggerCount = 0;

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  ScareCrow ESP32 Firmware v3.0");
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
    
    // Try to connect to saved WiFi
    if (wifiSSID.length() > 0) {
        Serial.printf("Connecting to WiFi: %s\n", wifiSSID.c_str());
        blinkLED(3, 100);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
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
            Serial.println("\n✗ WiFi connection failed");
        }
    }
    
    // Start captive portal if not connected
    if (!wifiConnected) {
        startCaptivePortal();
    }
    
    Serial.println("\n========================================");
    Serial.println("Setup complete!");
    Serial.printf("Sensor on GPIO %d, threshold %d\n", PIN_SENSOR, SENSOR_THRESHOLD);
    Serial.printf("Cooldown: %d seconds\n", COOLDOWN_SECONDS);
    Serial.println("========================================\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Handle web server
    server.handleClient();
    
    // Handle DNS in portal mode
    if (inPortalMode) {
        dnsServer.processNextRequest();
        
        // Blink LED in portal mode
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 1000) {
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
            lastBlink = millis();
        }
        return;
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
    
    // Periodic config fetch
    if (wifiConnected && millis() - lastConfigFetch > CONFIG_FETCH_INTERVAL) {
        fetchConfig();
        lastConfigFetch = millis();
    }
    
    delay(100);
}

// ==================== CONFIG ====================
void loadConfig() {
    deviceId = prefs.getString("deviceId", "");
    serverUrl = prefs.getString("serverUrl", DEFAULT_SERVER_URL);
    wifiSSID = prefs.getString("wifiSSID", "");
    wifiPass = prefs.getString("wifiPass", "");
    
    // Generate device ID if empty
    if (deviceId.length() == 0) {
        uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
        deviceId = "SCARECROW-" + String(chipId, HEX);
        deviceId.toUpperCase();
        prefs.putString("deviceId", deviceId);
    }
    
    Serial.printf("Device: %s\n", deviceId.c_str());
    Serial.printf("Server: %s\n", serverUrl.c_str());
}

void saveConfig() {
    prefs.putString("serverUrl", serverUrl);
    prefs.putString("wifiSSID", wifiSSID);
    prefs.putString("wifiPass", wifiPass);
    Serial.println("✓ Config saved");
}

void clearConfig() {
    prefs.clear();
    Serial.println("✓ Config cleared - restarting...");
    delay(1000);
    ESP.restart();
}

void fetchConfig() {
    String url = serverUrl + "/devices/" + deviceId + "/config";
    
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    
    if (code == 200) {
        Serial.println("✓ Config fetched from server");
    } else {
        Serial.printf("Config fetch failed: %d\n", code);
    }
    http.end();
}

// ==================== CAPTIVE PORTAL ====================
void startCaptivePortal() {
    Serial.println("Starting Captive Portal...");
    inPortalMode = true;
    
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configure AP
    IPAddress ip(192, 168, 4, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    
    String apName = String(AP_NAME_PREFIX) + deviceId.substring(10);
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    delay(500);
    
    // Start DNS - redirect all to our IP
    dnsServer.start(53, "*", ip);
    
    // Setup routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/reset", HTTP_GET, handleReset);
    
    // Captive portal detection endpoints
    server.on("/generate_204", HTTP_GET, handleRedirect);
    server.on("/gen_204", HTTP_GET, handleRedirect);
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    server.on("/ncsi.txt", HTTP_GET, handleRedirect);
    server.on("/connecttest.txt", HTTP_GET, handleRedirect);
    server.on("/fwlink", HTTP_GET, handleRedirect);
    server.onNotFound(handleRoot);
    
    server.begin();
    
    Serial.printf("✓ AP: %s\n", apName.c_str());
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.println("  Open http://192.168.4.1");
}

void handleRedirect() {
    // Send actual HTML page with multiple redirect methods
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'>";
    html += "<script>window.location.replace('http://192.168.4.1/');</script>";
    html += "</head><body>";
    html += "<p>Redirecting... <a href='http://192.168.4.1/'>Click here</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleRoot() {
    Serial.println(">> Serving portal page");
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ScareCrow Setup</title>";
    html += "<style>";
    html += "body{font-family:sans-serif;background:#1a1a2e;color:#fff;padding:20px;margin:0}";
    html += ".box{max-width:400px;margin:0 auto}";
    html += "h1{color:#4ade80;text-align:center}";
    html += ".id{background:#16213e;border:1px solid #4ade80;border-radius:8px;padding:10px;text-align:center;margin:20px 0;font-family:monospace}";
    html += ".card{background:#16213e;border-radius:12px;padding:20px;margin:15px 0}";
    html += "label{display:block;margin:10px 0 5px;color:#94a3b8}";
    html += "select,input{width:100%;padding:12px;border:1px solid #445;border-radius:8px;background:#0a0a12;color:#fff;box-sizing:border-box}";
    html += ".btn{width:100%;padding:14px;border:none;border-radius:8px;font-size:16px;cursor:pointer;margin:10px 0}";
    html += ".btn-green{background:#22c55e;color:#000;font-weight:bold}";
    html += ".btn-gray{background:#334;color:#fff}";
    html += ".btn-red{background:#dc2626;color:#fff}";
    html += "</style></head><body><div class='box'>";
    
    html += "<h1>ScareCrow Setup</h1>";
    html += "<div class='id'>" + deviceId + "</div>";
    
    html += "<form method='POST' action='/save'>";
    html += "<div class='card'>";
    html += "<label>WiFi Network</label>";
    html += "<select name='ssid' id='ssid' required><option>Loading...</option></select>";
    html += "<button type='button' class='btn btn-gray' onclick='scan()'>Refresh</button>";
    html += "<label>WiFi Password</label>";
    html += "<input type='password' name='pass' placeholder='Enter password'>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<label>Server URL</label>";
    html += "<input type='text' name='server' value='" + serverUrl + "'>";
    html += "<small style='color:#666'>Your computer's IP running the backend</small>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-green'>Save & Connect</button>";
    html += "</form>";
    
    html += "<button class='btn btn-red' onclick=\"if(confirm('Reset all settings?'))location='/reset'\">Reset Config</button>";
    
    html += "<script>";
    html += "function scan(){";
    html += "var s=document.getElementById('ssid');";
    html += "s.innerHTML='<option>Scanning...</option>';";
    html += "fetch('/scan').then(r=>r.json()).then(d=>{";
    html += "s.innerHTML='<option value=\"\">Select network</option>';";
    html += "d.forEach(n=>{var o=document.createElement('option');o.value=n.ssid;o.text=n.ssid+(n.sec?' 🔒':'');s.add(o);});";
    html += "}).catch(()=>s.innerHTML='<option>Scan failed</option>');";
    html += "}";
    html += "setTimeout(scan,500);";
    html += "</script>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
    Serial.println("<< Page sent");
}

void handleScan() {
    Serial.println("Scanning WiFi...");
    int n = WiFi.scanNetworks();
    
    String json = "[";
    for (int i = 0; i < n && i < 15; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + ",\"sec\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    
    WiFi.scanDelete();
    server.send(200, "application/json", json);
    Serial.printf("Found %d networks\n", n);
}

void handleSave() {
    if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
        wifiSSID = server.arg("ssid");
        wifiPass = server.arg("pass");
        serverUrl = server.arg("server");
        
        saveConfig();
        
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<style>body{font-family:sans-serif;background:#1a1a2e;color:#fff;text-align:center;padding:50px}</style>";
        html += "</head><body><h1>✓ Saved!</h1><p>Connecting to " + wifiSSID + "...</p>";
        html += "<p>Device will restart in 3 seconds.</p></body></html>";
        
        server.send(200, "text/html", html);
        delay(3000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing SSID");
    }
}

void handleReset() {
    server.send(200, "text/html", "<!DOCTYPE html><html><body style='background:#1a1a2e;color:#fff;text-align:center;padding:50px'><h1>Resetting...</h1></body></html>");
    delay(1000);
    clearConfig();
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
    Serial.println("⚠️ Activating deterrent...");
    
    // Wave servos
    for (int i = 0; i < 3; i++) {
        servo1.write(45);
        servo2.write(135);
        delay(300);
        servo1.write(135);
        servo2.write(45);
        delay(300);
    }
    servo1.write(90);
    servo2.write(90);
    
    // Beep buzzer
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_BUZZER, HIGH);
        delay(100);
        digitalWrite(PIN_BUZZER, LOW);
        delay(100);
    }
    
    // Flash LED
    blinkLED(10, 50);
    
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
