/*
 * MvsConnect - Basic Example (Custom Web Page)
 * =============================================
 *
 * This example shows how to:
 * 1. Serve a custom web page on ESP32
 * 2. Control an LED via custom HTML button
 * 3. Transfer WiFi credentials from Android app
 * 4. Enable over-the-air firmware updates
 *
 * LIBRARIES REQUIRED:
 * - MvsConnect (this library) - Web server + WiFi credential transfer
 * - MvsOTA_ESP32 - Over-the-air firmware updates (optional)
 *
 * HARDWARE:
 * - ESP32 board
 * - LED on GPIO 2 (built-in on most boards)
 *
 * HOW TO USE:
 * 1. Upload this sketch to ESP32
 * 2. Install mvsConnect Android app
 * 3. In app: Click "Find Devices" to discover ESP32
 * 4. Control the LED from the custom web page
 * 5. Use "Transfer WiFi" to send your home WiFi to ESP32
 */

#include <WiFi.h>
#include <MvsConnect.h>
#include <mvsota_esp32.h>    // Optional: OTA firmware updates

// ============================================
// DEVICE CONFIGURATION - CUSTOMIZE THESE!
// ============================================
#define DEVICE_NAME     "MyESP32"
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================
// AP SSID MUST end with "_mvstech" for Android app discovery!
#define AP_SUFFIX       "_mvstech"
#define AP_PASSWORD     "mvstech9867"   // Min 8 chars

// ============================================
// USER-AGENT AUTHENTICATION (OPTIONAL)
// ============================================
// Uncomment to only allow requests from the official app:
// #define ENABLE_USER_AGENT_AUTH  true
// #define EXPECTED_USER_AGENT     "MVStech7689"

// ============================================
// HARDWARE
// ============================================
#define LED_PIN  2  // Built-in LED

// ============================================
// LIBRARY INSTANCES
// ============================================
MvsConnect device(DEVICE_NAME, FIRMWARE_VERSION);
MvsOTA mvsota;  // Optional

// LED state
bool ledState = false;

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  MvsConnect - Basic Example");
    Serial.println("========================================");

    // Setup LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // WiFi: AP + Station mode
    WiFi.mode(WIFI_AP_STA);
    String apName = String(DEVICE_NAME) + AP_SUFFIX;
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    Serial.printf("AP: %s (IP: %s)\n", apName.c_str(), WiFi.softAPIP().toString().c_str());

    // -----------------------------------------
    // Custom Web Page (full HTML)
    // -----------------------------------------
    // setCustomHTML() takes over the entire "/" page
    // You provide a complete HTML page - perfect for PWA or custom UI

    device.setCustomHTML([]() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>" + device.getDeviceName() + "</title>";
        html += "<style>";
        html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
        html += "body { font-family: -apple-system, sans-serif; background: #1a1a2e; color: #eee; ";
        html += "min-height: 100vh; display: flex; flex-direction: column; align-items: center; }";
        html += ".header { background: linear-gradient(135deg, #667eea, #764ba2); width: 100%; ";
        html += "padding: 20px; text-align: center; }";
        html += ".header h1 { font-size: 22px; }";
        html += ".container { padding: 30px; max-width: 400px; width: 100%; }";
        html += ".card { background: #16213e; border-radius: 12px; padding: 25px; text-align: center; ";
        html += "box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".led-icon { font-size: 64px; margin: 20px 0; }";
        html += ".btn { width: 100%; padding: 14px; border: none; border-radius: 8px; ";
        html += "font-size: 18px; font-weight: bold; cursor: pointer; margin-top: 15px; }";
        html += ".btn-on { background: #667eea; color: white; }";
        html += ".btn-off { background: #394867; color: #aaa; }";
        html += ".status { margin-top: 10px; font-size: 14px; opacity: 0.7; }";
        html += "</style></head><body>";

        html += "<div class='header'><h1>" + device.getDeviceName() + "</h1>";
        html += "<small>v" + device.getVersion() + "</small></div>";

        html += "<div class='container'><div class='card'>";
        html += "<div class='led-icon'>" + String(ledState ? "&#128161;" : "&#9899;") + "</div>";
        html += "<h2>LED is " + String(ledState ? "ON" : "OFF") + "</h2>";
        html += "<button class='btn " + String(ledState ? "btn-off" : "btn-on") + "' ";
        html += "onclick=\"fetch('/api/toggle').then(()=>location.reload())\">";
        html += String(ledState ? "Turn OFF" : "Turn ON") + "</button>";
        html += "<div class='status'>GPIO " + String(LED_PIN) + "</div>";
        html += "</div></div>";

        html += "</body></html>";
        return html;
    });

    // -----------------------------------------
    // Custom API endpoint for LED toggle
    // -----------------------------------------
    device.addEndpoint("/api/toggle", []() {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        Serial.printf("LED toggled: %s\n", ledState ? "ON" : "OFF");
        device.getServer()->send(200, "text/plain", ledState ? "ON" : "OFF");
    });

    // -----------------------------------------
    // Start services
    // -----------------------------------------
    device.begin();
    mvsota.begin(DEVICE_NAME, FIRMWARE_VERSION);

    // Optional: User-Agent auth
    #ifdef ENABLE_USER_AGENT_AUTH
        #ifdef EXPECTED_USER_AGENT
            device.setUserAgentAuth(true, EXPECTED_USER_AGENT);
        #else
            device.setUserAgentAuth(true);
        #endif
    #endif

    // Callback when WiFi credentials received from app
    device.onWiFiCredentialsReceived([](const String& ssid) {
        Serial.printf("Received WiFi credentials for: %s\n", ssid.c_str());
    });

    // Try saved WiFi
    if (device.connectToSavedWiFi(10000)) {
        Serial.printf("Connected to WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("No saved WiFi - use app to transfer credentials");
    }

    // OTA callbacks (optional)
    mvsota.onStart([]() {
        Serial.println("OTA Update Starting...");
        digitalWrite(LED_PIN, LOW);
    });

    Serial.println("\n========================================");
    Serial.println("  Setup Complete!");
    Serial.printf("  Web UI: http://%s:%d/\n", WiFi.softAPIP().toString().c_str(), device.getPort());
    Serial.println("========================================\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    device.handle();

    if (!mvsota.isUpdating()) {
        mvsota.handle();
    }

    delay(10);
}
