/*
 * ScareCrowWeb ESP32 Firmware v3.0 - User-Friendly Edition
 * 
 * IMPROVEMENTS OVER v2:
 * - WiFi network scanning with dropdown selection (no typing WiFi name!)
 * - mDNS support (connect to scarecrow.local instead of IP addresses)
 * - Status LED patterns (visual feedback without serial monitor)
 * - Local status webpage (access device status from browser)
 * - One-click config reset (hold button to reset)
 * 
 * REQUIRED LIBRARIES (install via Arduino Library Manager):
 * - WiFiManager by tzapu (optional, not used in this version)
 * - ArduinoJson by Benoit Blanchon
 * - ESP32Servo by Kevin Harrington
 * - ESPmDNS (built-in)
 * 
 * Board Setup:
 * - Board: ESP32S3 Dev Module
 * - PSRAM: OPI PSRAM
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

// Include board-specific configurations
#include "camera_pins.h"
#include "config.h"

// ==================== Status LED Class ====================
// Non-blocking LED pattern management
class StatusLED {
private:
    uint8_t pin;
    bool ledState;
    unsigned long lastUpdate;
    int onTime;
    int offTime;
    int blinkCount;
    int currentBlinks;
    bool patternActive;
    
public:
    StatusLED(uint8_t ledPin) : pin(ledPin), ledState(false), lastUpdate(0), 
                                 onTime(0), offTime(0), blinkCount(-1), 
                                 currentBlinks(0), patternActive(false) {}
    
    void begin() {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    
    void setPattern(int on, int off, int count = -1) {
        onTime = on;
        offTime = off;
        blinkCount = count;
        currentBlinks = 0;
        patternActive = true;
        lastUpdate = millis();
        ledState = true;
        digitalWrite(pin, HIGH);
    }
    
    void solid(bool on) {
        patternActive = false;
        ledState = on;
        digitalWrite(pin, on ? HIGH : LOW);
    }
    
    void off() {
        solid(false);
    }
    
    void update() {
        if (!patternActive) return;
        
        unsigned long now = millis();
        int interval = ledState ? onTime : offTime;
        
        if (now - lastUpdate >= interval) {
            lastUpdate = now;
            
            if (ledState && blinkCount > 0) {
                 currentBlinks++;
                if (currentBlinks >= blinkCount) {
                    patternActive = false;
                    ledState = false;
                    digitalWrite(pin, LOW);
                    return;
                }
            }
            
            ledState = !ledState;
            digitalWrite(pin, ledState ? HIGH : LOW);
        }
    }
    
    // Pre-defined patterns
    void patternPortal() { setPattern(LED_PATTERN_PORTAL_ON, LED_PATTERN_PORTAL_OFF); }
    void patternConnecting() { setPattern(LED_PATTERN_CONNECT_ON, LED_PATTERN_CONNECT_OFF); }
    void patternSuccess() { solid(true); }
    void patternError() { setPattern(LED_PATTERN_ERROR_ON, LED_PATTERN_ERROR_OFF, LED_PATTERN_ERROR_COUNT); }
    void patternUpload() { setPattern(LED_PATTERN_UPLOAD_ON, LED_PATTERN_UPLOAD_OFF, 3); }
};

// ==================== Global Objects ====================
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
HTTPClient http;
StatusLED statusLed(PIN_LED);

Servo servo1;
Servo servo2;

// ==================== Configuration ====================
struct DeviceConfig {
    char deviceId[32];
    char serverUrl[128];
    char wifiSSID[64];
    char wifiPassword[64];
    bool useMdns;
    
    // Sensor settings
    int sensorType;          // 0 = PIR digital, 1 = analog (1V sensor)
    int analogThreshold;     // ADC threshold for analog sensor
    int cooldownSec;         // Cooldown between triggers (60-600 seconds)
    int captureDelayMs;      // Delay before capture after trigger
    
    // Actuator settings (fetched from server)
    int servoSpeed;
    int servoAngle;
    int servoDuration;
    String ledPattern;
    int ledSpeedMs;
    int ledDurationSec;
    String soundType;
    int soundVolume;
    int soundDurationSec;
    bool autoDeterrent;
};

DeviceConfig config;

// ==================== State Variables ====================
bool isConfigMode = false;
bool isConnected = false;
bool serverReachable = false;
unsigned long lastConfigFetch = 0;
unsigned long lastMotionDetected = 0;
unsigned long lastServerCheck = 0;
unsigned long configFetchInterval = 30000;
unsigned long buttonPressStart = 0;
bool buttonPressed = false;

// Statistics for status page
unsigned long lastUploadTime = 0;
unsigned long lastDetectionTime = 0;
int uploadCount = 0;
int detectionCount = 0;

// ==================== Forward Declarations ====================
void setupCamera();
void setupActuators();
void loadConfig();
void saveConfig();
void clearConfig();
void startCaptivePortal();
void startStatusServer();
void handlePortalRoot();
void handlePortalSave();
void handleWifiScan();
void handleStatusPage();
void handleTestCapture();
void handleResetConfig();
void handleApiStatus();
bool connectToWiFi();
bool resolveServerMdns();
void fetchRemoteConfig();
void captureAndUpload();
void uploadImage(uint8_t* imageData, size_t imageLen);
void activateDeterrent();
void waveServos();
void flashLED();
void playAlarm();
void checkResetButton();

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n========================================");
    Serial.println("   ScareCrowWeb ESP32 Firmware v3.0");
    Serial.println("      (User-Friendly Edition)");
    Serial.println("========================================");
    
    // Initialize status LED first (for visual feedback)
    statusLed.begin();
    statusLed.patternConnecting();
    
    // Initialize preferences
    preferences.begin("scarecrow", false);
    loadConfig();
    
    // Check if reset button held at boot
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    if (digitalRead(PIN_RESET_BUTTON) == LOW) {
        Serial.println("Reset button held at boot - clearing config...");
        statusLed.patternError();
        delay(2000);
        clearConfig();
        ESP.restart();
    }
    
    // Setup actuators
    setupActuators();
    delay(500);
    
    // Setup camera
    Serial.println("Preparing camera...");
    delay(1000);
    setupCamera();
    delay(500);
    
    // Try to connect to WiFi if credentials exist
    if (strlen(config.wifiSSID) > 0) {
        Serial.printf("Connecting to WiFi: %s\n", config.wifiSSID);
        statusLed.patternConnecting();
        
        if (connectToWiFi()) {
            Serial.println("✓ Connected to WiFi!");
            Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
            isConnected = true;
            statusLed.patternSuccess();
            
            // Start mDNS for this device
            String deviceMdns = String(MDNS_DEVICE_PREFIX) + String(config.deviceId).substring(10);
            if (MDNS.begin(deviceMdns.c_str())) {
                Serial.printf("  mDNS: %s.local\n", deviceMdns.c_str());
            }
            
            // Try to resolve server via mDNS
            if (config.useMdns) {
                resolveServerMdns();
            }
            
            // Fetch initial config
            fetchRemoteConfig();
            
            // Start status server
            startStatusServer();
            
            delay(2000);
            statusLed.off();
        } else {
            Serial.println("✗ WiFi connection failed");
            statusLed.patternError();
            delay(2000);
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
    // Update status LED
    statusLed.update();
    
    // Check reset button
    checkResetButton();
    
    // Handle web server requests
    server.handleClient();
    
    // Handle DNS if in captive portal mode
    if (isConfigMode) {
        dnsServer.processNextRequest();
        return;
    }
    
    // Check sensor for motion (supports both PIR digital and analog sensors)
    bool motionDetected = false;
    
    if (config.sensorType == 0) {
        // PIR digital sensor - reads HIGH when triggered
        motionDetected = (digitalRead(PIN_SENSOR) == HIGH);
    } else {
        // Analog sensor (e.g., 1V output for 20 seconds)
        int analogValue = analogRead(PIN_SENSOR);
        motionDetected = (analogValue >= config.analogThreshold);
        
        // Debug analog reading periodically
        static unsigned long lastAnalogLog = 0;
        if (millis() - lastAnalogLog > 5000) {
            Serial.printf("📊 Sensor reading: %d (threshold: %d)\n", analogValue, config.analogThreshold);
            lastAnalogLog = millis();
        }
    }
    
    if (motionDetected) {
        unsigned long currentTime = millis();
        unsigned long cooldownMs = (unsigned long)config.cooldownSec * 1000UL;
        
        if (currentTime - lastMotionDetected > cooldownMs) {
            Serial.println("🚨 Motion detected!");
            Serial.printf("   Cooldown: %d seconds\n", config.cooldownSec);
            detectionCount++;
            lastDetectionTime = currentTime;
            
            delay(config.captureDelayMs);
            
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
    if (isConnected && millis() - lastConfigFetch > configFetchInterval) {
        fetchRemoteConfig();
        lastConfigFetch = millis();
    }
    
    // Periodically check server reachability
    if (isConnected && millis() - lastServerCheck > 60000) {
        checkServerReachable();
        lastServerCheck = millis();
    }
    
    delay(100);
}

// ==================== RESET BUTTON CHECK ====================
void checkResetButton() {
    if (digitalRead(PIN_RESET_BUTTON) == LOW) {
        if (!buttonPressed) {
            buttonPressed = true;
            buttonPressStart = millis();
        } else if (millis() - buttonPressStart > RESET_BUTTON_HOLD_TIME) {
            Serial.println("Reset button held - clearing configuration...");
            statusLed.patternError();
            delay(1000);
            clearConfig();
            ESP.restart();
        }
    } else {
        buttonPressed = false;
    }
}

// ==================== CONFIG MANAGEMENT ====================
void loadConfig() {
    Serial.println("Loading configuration...");
    
    String deviceId = preferences.getString("deviceId", "");
    String serverUrl = preferences.getString("serverUrl", DEFAULT_SERVER_URL);
    String wifiSSID = preferences.getString("wifiSSID", "");
    String wifiPassword = preferences.getString("wifiPassword", "");
    bool useMdns = preferences.getBool("useMdns", MDNS_ENABLED);
    
    strncpy(config.deviceId, deviceId.c_str(), sizeof(config.deviceId) - 1);
    strncpy(config.serverUrl, serverUrl.c_str(), sizeof(config.serverUrl) - 1);
    strncpy(config.wifiSSID, wifiSSID.c_str(), sizeof(config.wifiSSID) - 1);
    strncpy(config.wifiPassword, wifiPassword.c_str(), sizeof(config.wifiPassword) - 1);
    config.useMdns = useMdns;
    
    // Generate device ID if not set
    if (strlen(config.deviceId) == 0) {
        uint64_t chipId = ESP.getEfuseMac();
        snprintf(config.deviceId, sizeof(config.deviceId), "SCARECROW-%04X", (uint16_t)(chipId & 0xFFFF));
    }
    
    // Set defaults for sensor config
    config.sensorType = DEFAULT_SENSOR_TYPE;       // 1 = analog sensor
    config.analogThreshold = ANALOG_SENSOR_THRESHOLD;  // ~800 for 1V
    config.cooldownSec = DEFAULT_COOLDOWN_SEC;     // 60 seconds (1 min)
    config.captureDelayMs = 500;                   // 500ms delay before capture
    
    // Set defaults for actuator config
    config.servoSpeed = 90;
    config.servoAngle = 180;
    config.servoDuration = 3;
    config.ledPattern = "blink";
    config.ledSpeedMs = 200;
    config.ledDurationSec = 5;
    config.soundType = "beep";
    config.soundVolume = 80;
    config.soundDurationSec = 3;
    config.autoDeterrent = true;
    
    Serial.printf("Device ID: %s\n", config.deviceId);
    Serial.printf("Server URL: %s\n", config.serverUrl);
    Serial.printf("Sensor Type: %s\n", config.sensorType == 0 ? "PIR Digital" : "Analog (1V)");
    Serial.printf("Cooldown: %d seconds\n", config.cooldownSec);
}

void saveConfig() {
    preferences.putString("deviceId", config.deviceId);
    preferences.putString("serverUrl", config.serverUrl);
    preferences.putString("wifiSSID", config.wifiSSID);
    preferences.putString("wifiPassword", config.wifiPassword);
    preferences.putBool("useMdns", config.useMdns);
    Serial.println("✓ Configuration saved");
}

void clearConfig() {
    preferences.clear();
    Serial.println("✓ Configuration cleared");
}

// ==================== WIFI CONNECTION ====================
bool connectToWiFi() {
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSSID, config.wifiPassword);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) { // 20 seconds
        delay(500);
        Serial.print(".");
        statusLed.update();
        attempts++;
    }
    Serial.println();
    
    return WiFi.status() == WL_CONNECTED;
}

// ==================== mDNS RESOLUTION ====================
bool resolveServerMdns() {
    Serial.printf("Resolving %s.local via mDNS...\n", MDNS_SERVER_NAME);
    
    IPAddress serverIp = MDNS.queryHost(MDNS_SERVER_NAME);
    
    if (serverIp != IPAddress(0, 0, 0, 0)) {
        Serial.printf("✓ Found server at: %s\n", serverIp.toString().c_str());
        snprintf(config.serverUrl, sizeof(config.serverUrl), 
                 "http://%s:%d", serverIp.toString().c_str(), DEFAULT_SERVER_PORT);
        serverReachable = true;
        return true;
    } else {
        Serial.println("✗ mDNS resolution failed, using configured URL");
        return false;
    }
}

void checkServerReachable() {
    http.begin(String(config.serverUrl) + "/");
    int httpCode = http.GET();
    serverReachable = (httpCode == 200);
    http.end();
    
    if (!serverReachable) {
        statusLed.patternError();
    }
}

// ==================== CAPTIVE PORTAL ====================
void startCaptivePortal() {
    Serial.println("Starting Captive Portal...");
    isConfigMode = true;
    statusLed.patternPortal();
    
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);
    delay(100);
    
    String apName = "ScareCrow-" + String(config.deviceId).substring(10);
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    delay(500);
    
    // Start DNS server - redirect ALL domains to our IP
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    // ===== Setup captive portal routes =====
    // Main page
    server.on("/", HTTP_GET, handlePortalRoot);
    server.on("/save", HTTP_POST, handlePortalSave);
    server.on("/scan", HTTP_GET, handleWifiScan);
    
    // Android connectivity checks
    server.on("/generate_204", HTTP_GET, []() {
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
    });
    server.on("/gen_204", HTTP_GET, []() {
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
    });
    
    // Apple/iOS connectivity checks  
    server.on("/hotspot-detect.html", HTTP_GET, handlePortalRoot);
    server.on("/library/test/success.html", HTTP_GET, handlePortalRoot);
    
    // Windows connectivity checks
    server.on("/ncsi.txt", HTTP_GET, handlePortalRoot);
    server.on("/connecttest.txt", HTTP_GET, []() {
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
    });
    server.on("/redirect", HTTP_GET, handlePortalRoot);
    
    // Microsoft
    server.on("/fwlink", HTTP_GET, handlePortalRoot);
    
    // Catch all other requests
    server.onNotFound(handlePortalRoot);
    
    server.begin();
    
    Serial.printf("✓ AP Started: %s\n", apName.c_str());
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.println("  Open: http://192.168.4.1");
    Serial.println("  (Captive portal should auto-popup on most devices)");
}

// ==================== CAPTIVE PORTAL HANDLERS ====================
void handleWifiScan() {
    Serial.println("Scanning WiFi networks...");
    
    int n = WiFi.scanNetworks();
    
    StaticJsonDocument<2048> doc;
    JsonArray networks = doc.createNestedArray("networks");
    
    for (int i = 0; i < n && i < 15; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    
    WiFi.scanDelete();
}

void handlePortalRoot() {
    Serial.println(">> Serving captive portal page...");
    
    // Send response in chunks to avoid memory issues
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    // Part 1: Head
    server.sendContent(F("<!DOCTYPE html><html><head>"));
    server.sendContent(F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"));
    server.sendContent(F("<title>ScareCrow Setup</title>"));
    server.sendContent(F("<style>"));
    server.sendContent(F("*{box-sizing:border-box;margin:0;padding:0}"));
    server.sendContent(F("body{font-family:sans-serif;background:#1a1a2e;color:#fff;min-height:100vh;padding:20px}"));
    server.sendContent(F(".c{max-width:400px;margin:0 auto}"));
    server.sendContent(F("h1{color:#4ade80;text-align:center;margin-bottom:20px}"));
    server.sendContent(F(".id{background:#1e3a5f;border:1px solid #4ade80;border-radius:8px;padding:10px;text-align:center;margin-bottom:20px;font-family:monospace}"));
    server.sendContent(F(".box{background:#16213e;border:1px solid #334;border-radius:12px;padding:20px;margin-bottom:15px}"));
    server.sendContent(F("label{display:block;margin-bottom:6px;color:#94a3b8}"));
    server.sendContent(F("select,input{width:100%;padding:12px;border:1px solid #445;border-radius:8px;background:#0a0a12;color:#fff;margin-bottom:10px}"));
    server.sendContent(F(".btn{width:100%;padding:14px;border:none;border-radius:8px;font-size:16px;cursor:pointer;margin-bottom:10px}"));
    server.sendContent(F(".btn-p{background:#22c55e;color:#000;font-weight:bold}"));
    server.sendContent(F(".btn-s{background:#334;color:#fff}"));
    server.sendContent(F("</style></head><body><div class='c'>"));
    
    // Part 2: Header and Device ID
    server.sendContent(F("<h1>ScareCrow Setup</h1>"));
    server.sendContent("<div class='id'>" + String(config.deviceId) + "</div>");
    
    // Part 3: Form
    server.sendContent(F("<form method='POST' action='/save'>"));
    server.sendContent(F("<div class='box'><label>WiFi Network</label>"));
    server.sendContent(F("<select name='ssid' id='w' required><option value=''>Loading...</option></select>"));
    server.sendContent(F("<button type='button' class='btn btn-s' onclick='scan()'>Refresh Networks</button>"));
    server.sendContent(F("<label>WiFi Password</label><input type='password' name='password' placeholder='Enter password'>"));
    server.sendContent(F("</div>"));
    
    server.sendContent(F("<div class='box'><label>Server URL</label>"));
    server.sendContent("<input type='text' name='server' value='" + String(config.serverUrl) + "'>");
    server.sendContent(F("</div>"));
    
    server.sendContent(F("<button type='submit' class='btn btn-p'>Save & Connect</button>"));
    server.sendContent(F("</form>"));
    
    // Part 4: Script
    server.sendContent(F("<script>"));
    server.sendContent(F("function scan(){var s=document.getElementById('w');s.innerHTML='<option>Scanning...</option>';"));
    server.sendContent(F("fetch('/scan').then(r=>r.json()).then(d=>{s.innerHTML='<option value=\"\">-- Select --</option>';"));
    server.sendContent(F("d.networks.forEach(n=>{var o=document.createElement('option');o.value=n.ssid;o.text=n.ssid+(n.secure?' 🔒':'');s.add(o);});"));
    server.sendContent(F("}).catch(()=>{s.innerHTML='<option>Scan failed</option>';});}"));
    server.sendContent(F("setTimeout(scan,500);"));
    server.sendContent(F("</script></div></body></html>"));
    
    Serial.println("<< Portal page sent successfully");
}

void handlePortalSave() {
    if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
        strncpy(config.wifiSSID, server.arg("ssid").c_str(), sizeof(config.wifiSSID) - 1);
    }
    if (server.hasArg("password")) {
        strncpy(config.wifiPassword, server.arg("password").c_str(), sizeof(config.wifiPassword) - 1);
    }
    if (server.hasArg("server")) {
        strncpy(config.serverUrl, server.arg("server").c_str(), sizeof(config.serverUrl) - 1);
    }
    config.useMdns = server.hasArg("mdns");
    
    saveConfig();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Saved!</title>
    <style>
        body {
            font-family: -apple-system, sans-serif;
            background: linear-gradient(135deg, #1a1a2e, #16213e);
            color: #fff;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            text-align: center;
        }
        .success { font-size: 64px; }
        h1 { margin: 20px 0 10px; }
        p { color: #94a3b8; }
    </style>
</head>
<body>
    <div>
        <div class="success">✅</div>
        <h1>Settings Saved!</h1>
        <p>Device will restart and connect to your network...</p>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
    delay(2000);
    ESP.restart();
}

// ==================== STATUS SERVER (AFTER WIFI CONNECTED) ====================
void startStatusServer() {
    server.on("/", HTTP_GET, handleStatusPage);
    server.on("/test", HTTP_GET, handleTestCapture);
    server.on("/reset", HTTP_GET, handleResetConfig);
    server.on("/api/status", HTTP_GET, handleApiStatus);
    
    server.begin();
    Serial.println("✓ Status server started");
}

void handleStatusPage() {
    String serverStatus = serverReachable ? "✅ Connected" : "❌ Unreachable";
    String lastDetect = lastDetectionTime > 0 ? String((millis() - lastDetectionTime) / 1000) + "s ago" : "Never";
    String lastUpload = lastUploadTime > 0 ? String((millis() - lastUploadTime) / 1000) + "s ago" : "Never";
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="10">
    <title>ScareCrow Status</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #fff;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 500px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 20px; }
        .header h1 { font-size: 24px; color: #4ade80; }
        .card {
            background: rgba(255,255,255,0.05);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 15px;
        }
        .card h2 { font-size: 14px; color: #94a3b8; margin-bottom: 10px; }
        .stat { font-size: 24px; font-weight: 600; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        .status-row {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .status-row:last-child { border-bottom: none; }
        .btn {
            display: block;
            width: 100%;
            padding: 14px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            text-decoration: none;
            text-align: center;
            margin-bottom: 10px;
        }
        .btn-primary { background: linear-gradient(135deg, #4ade80, #22c55e); color: #000; }
        .btn-danger { background: linear-gradient(135deg, #f87171, #ef4444); color: #fff; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🦅 ScareCrow Status</h1>
        </div>
        
        <div class="card">
            <h2>DEVICE INFO</h2>
            <div class="status-row">
                <span>Device ID</span>
                <strong>)rawliteral" + String(config.deviceId) + R"rawliteral(</strong>
            </div>
            <div class="status-row">
                <span>IP Address</span>
                <strong>)rawliteral" + WiFi.localIP().toString() + R"rawliteral(</strong>
            </div>
            <div class="status-row">
                <span>WiFi</span>
                <strong>)rawliteral" + String(config.wifiSSID) + R"rawliteral( (</strong>)rawliteral" + String(WiFi.RSSI()) + R"rawliteral( dBm)
            </div>
        </div>
        
        <div class="card">
            <h2>SERVER CONNECTION</h2>
            <div class="status-row">
                <span>Server URL</span>
                <strong>)rawliteral" + String(config.serverUrl) + R"rawliteral(</strong>
            </div>
            <div class="status-row">
                <span>Status</span>
                <strong>)rawliteral" + serverStatus + R"rawliteral(</strong>
            </div>
        </div>
        
        <div class="grid">
            <div class="card">
                <h2>DETECTIONS</h2>
                <div class="stat">)rawliteral" + String(detectionCount) + R"rawliteral(</div>
            </div>
            <div class="card">
                <h2>UPLOADS</h2>
                <div class="stat">)rawliteral" + String(uploadCount) + R"rawliteral(</div>
            </div>
        </div>
        
        <div class="card">
            <h2>ACTIVITY</h2>
            <div class="status-row">
                <span>Last Detection</span>
                <strong>)rawliteral" + lastDetect + R"rawliteral(</strong>
            </div>
            <div class="status-row">
                <span>Last Upload</span>
                <strong>)rawliteral" + lastUpload + R"rawliteral(</strong>
            </div>
        </div>
        
        <a href="/test" class="btn btn-primary">📷 Test Capture & Upload</a>
        <a href="/reset" class="btn btn-danger" onclick="return confirm('Reset all settings?')">🔄 Reset Configuration</a>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void handleTestCapture() {
    captureAndUpload();
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleResetConfig() {
    clearConfig();
    server.send(200, "text/html", "<h1>Configuration cleared. Restarting...</h1>");
    delay(1000);
    ESP.restart();
}

void handleApiStatus() {
    StaticJsonDocument<512> doc;
    doc["device_id"] = config.deviceId;
    doc["connected"] = isConnected;
    doc["server_reachable"] = serverReachable;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["detections"] = detectionCount;
    doc["uploads"] = uploadCount;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ==================== CAMERA SETUP ====================
void setupCamera() {
    Serial.println("Initializing camera...");
    
    bool hasPSRAM = psramFound();
    Serial.printf("PSRAM: %s\n", hasPSRAM ? "Found" : "Not found");
    
    camera_config_t cameraConfig;
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
    cameraConfig.xclk_freq_hz = 10000000;
    cameraConfig.pixel_format = PIXFORMAT_JPEG;
    cameraConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    cameraConfig.jpeg_quality = 12;
    cameraConfig.fb_count = 1;
    
    if (hasPSRAM) {
        cameraConfig.frame_size = FRAMESIZE_VGA;
        cameraConfig.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        cameraConfig.frame_size = FRAMESIZE_QVGA;
        cameraConfig.fb_location = CAMERA_FB_IN_DRAM;
    }
    
    esp_err_t err = esp_camera_init(&cameraConfig);
    
    if (err != ESP_OK && hasPSRAM) {
        Serial.println("PSRAM init failed, trying DRAM...");
        cameraConfig.frame_size = FRAMESIZE_QVGA;
        cameraConfig.fb_location = CAMERA_FB_IN_DRAM;
        err = esp_camera_init(&cameraConfig);
    }
    
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }
    
    Serial.println("✓ Camera initialized");
}

// ==================== ACTUATOR SETUP ====================
void setupActuators() {
    Serial.println("Initializing actuators...");
    
    // Initialize sensor pin based on type
    if (config.sensorType == 0) {
        // PIR digital sensor
        pinMode(PIN_SENSOR, INPUT);
        Serial.println("  Sensor: PIR (digital)");
    } else {
        // Analog sensor - no special initialization needed for ESP32 ADC
        // analogRead() works on any ADC-capable pin
        Serial.printf("  Sensor: Analog (threshold: %d)\n", config.analogThreshold);
    }
    
    // Buzzer
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // Servos
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    
    servo1.setPeriodHertz(50);
    servo2.setPeriodHertz(50);
    servo1.attach(PIN_SERVO1, 500, 2400);
    servo2.attach(PIN_SERVO2, 500, 2400);
    servo1.write(90);
    servo2.write(90);
    
    Serial.printf("  Cooldown: %d seconds (%d-%d min range)\n", 
                  config.cooldownSec, MIN_COOLDOWN_SEC/60, MAX_COOLDOWN_SEC/60);
    Serial.println("✓ Actuators initialized");
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
        serverReachable = true;
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error && doc["success"]) {
            JsonObject cfg = doc["config"];
            
            // Sensor settings (with bounds checking)
            int newCooldown = cfg["cooldown_sec"] | config.cooldownSec;
            config.cooldownSec = constrain(newCooldown, MIN_COOLDOWN_SEC, MAX_COOLDOWN_SEC);
            config.captureDelayMs = cfg["capture_delay_ms"] | config.captureDelayMs;
            config.analogThreshold = cfg["analog_threshold"] | config.analogThreshold;
            config.sensorType = cfg["sensor_type"] | config.sensorType;
            
            // Servo settings
            config.servoSpeed = cfg["servo_speed"] | 90;
            config.servoAngle = cfg["servo_angle"] | 180;
            config.servoDuration = cfg["servo_duration"] | 3;
            
            // LED settings
            config.ledPattern = cfg["led_pattern"] | "blink";
            config.ledSpeedMs = cfg["led_speed_ms"] | 200;
            config.ledDurationSec = cfg["led_duration_sec"] | 5;
            
            // Sound settings
            config.soundType = cfg["sound_type"] | "beep";
            config.soundVolume = cfg["sound_volume"] | 80;
            config.soundDurationSec = cfg["sound_duration_sec"] | 3;
            
            // Deterrent
            config.autoDeterrent = cfg["auto_deterrent"] | true;
            
            Serial.println("✓ Remote config updated");
            Serial.printf("  Cooldown: %d sec | Sensor: %s\n", 
                          config.cooldownSec, 
                          config.sensorType == 0 ? "PIR" : "Analog");
        }
    } else {
        Serial.printf("Config fetch failed, HTTP code: %d\n", httpCode);
        serverReachable = false;
    }
    
    http.end();
}

// ==================== IMAGE CAPTURE & UPLOAD ====================
void captureAndUpload() {
    Serial.println("📷 Capturing image...");
    statusLed.patternUpload();
    
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        statusLed.patternError();
        return;
    }
    
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    if (isConnected) {
        uploadImage(fb->buf, fb->len);
    }
    
    esp_camera_fb_return(fb);
    statusLed.off();
}

void uploadImage(uint8_t* imageData, size_t imageLen) {
    String url = String(config.serverUrl) + "/upload";
    Serial.printf("Uploading to: %s\n", url.c_str());
    
    String boundary = "----ScareCrowBoundary" + String(millis());
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
    bodyStart += String(config.deviceId) + "\r\n";
    bodyStart += "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    bodyStart += "Content-Type: image/jpeg\r\n\r\n";
    String bodyEnd = "\r\n--" + boundary + "--\r\n";
    
    WiFiClient client;
    
    String host = String(config.serverUrl);
    host.replace("http://", "");
    int portIdx = host.indexOf(':');
    int port = DEFAULT_SERVER_PORT;
    if (portIdx > 0) {
        port = host.substring(portIdx + 1).toInt();
        host = host.substring(0, portIdx);
    }
    
    if (!client.connect(host.c_str(), port)) {
        Serial.println("Connection to server failed");
        serverReachable = false;
        statusLed.patternError();
        return;
    }
    
    int contentLength = bodyStart.length() + imageLen + bodyEnd.length();
    
    client.println("POST /upload HTTP/1.1");
    client.println("Host: " + host);
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(contentLength));
    client.println("Connection: close");
    client.println();
    
    client.print(bodyStart);
    
    int chunkSize = 1024;
    for (size_t i = 0; i < imageLen; i += chunkSize) {
        size_t remaining = imageLen - i;
        size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;
        client.write(&imageData[i], toSend);
    }
    
    client.print(bodyEnd);
    
    unsigned long timeout = millis() + 10000;
    while (client.available() == 0) {
        if (millis() > timeout) {
            Serial.println("Upload timeout");
            client.stop();
            statusLed.patternError();
            return;
        }
        delay(100);
    }
    
    bool success = false;
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.indexOf("200") >= 0 || line.indexOf("success") >= 0) {
            success = true;
        }
    }
    
    client.stop();
    
    if (success) {
        Serial.println("✓ Upload successful");
        uploadCount++;
        lastUploadTime = millis();
        serverReachable = true;
    }
}

// ==================== DETERRENT ACTIONS ====================
void activateDeterrent() {
    Serial.println("🚨 Activating deterrent...");
    waveServos();
    flashLED();
    playAlarm();
    Serial.println("✓ Deterrent complete");
}

void waveServos() {
    unsigned long startTime = millis();
    unsigned long duration = config.servoDuration * 1000;
    
    while (millis() - startTime < duration) {
        for (int pos = 0; pos <= config.servoAngle; pos += 5) {
            servo1.write(pos);
            servo2.write(config.servoAngle - pos);
            delay(1000 / config.servoSpeed);
            if (millis() - startTime >= duration) break;
        }
        for (int pos = config.servoAngle; pos >= 0; pos -= 5) {
            servo1.write(pos);
            servo2.write(config.servoAngle - pos);
            delay(1000 / config.servoSpeed);
            if (millis() - startTime >= duration) break;
        }
    }
    
    servo1.write(90);
    servo2.write(90);
}

void flashLED() {
    unsigned long startTime = millis();
    unsigned long duration = config.ledDurationSec * 1000;
    
    if (config.ledPattern == "solid") {
        statusLed.solid(true);
        delay(duration);
        statusLed.off();
    } else {
        while (millis() - startTime < duration) {
            statusLed.solid(true);
            delay(config.ledSpeedMs);
            statusLed.solid(false);
            delay(config.ledSpeedMs);
        }
    }
}

void playAlarm() {
    unsigned long startTime = millis();
    unsigned long duration = config.soundDurationSec * 1000;
    
    if (config.soundType == "beep") {
        while (millis() - startTime < duration) {
            tone(PIN_BUZZER, 1000, 200);
            delay(300);
        }
    } else if (config.soundType == "siren") {
        while (millis() - startTime < duration) {
            for (int freq = 500; freq < 1500; freq += 50) {
                tone(PIN_BUZZER, freq, 20);
                delay(20);
            }
            for (int freq = 1500; freq > 500; freq -= 50) {
                tone(PIN_BUZZER, freq, 20);
                delay(20);
            }
        }
    }
    
    noTone(PIN_BUZZER);
}
