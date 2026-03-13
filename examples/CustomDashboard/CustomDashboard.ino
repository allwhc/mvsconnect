/*
 * MvsConnect - Custom Dashboard Example
 * ======================================
 *
 * This example demonstrates:
 * - Full custom web dashboard (HTML/CSS/JS)
 * - Custom REST API endpoints
 * - Real-time sensor data display
 * - GPIO control via custom buttons
 * - Auto-refresh with JavaScript fetch
 * - WiFi credential transfer (built into MvsConnect)
 * - Integration with MvsOTA_ESP32
 *
 * LIBRARIES REQUIRED:
 * - MvsConnect (this library) - Web server + WiFi transfer
 * - MvsOTA_ESP32 (optional) - OTA firmware updates
 *
 * HARDWARE:
 * - ESP32 board
 * - GPIO 2: Built-in LED
 * - GPIO 4: Relay (optional)
 * - GPIO 34: Analog sensor (optional, ADC input)
 */

#include <WiFi.h>
#include <MvsConnect.h>
#include <mvsota_esp32.h>

// ============================================
// DEVICE CONFIGURATION
// ============================================
#define DEVICE_NAME      "SmartHub"
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================
#define AP_SUFFIX        "_mvstech"     // Required for app discovery
#define AP_PASSWORD      "mvstech9867"

// ============================================
// USER-AGENT AUTHENTICATION (OPTIONAL)
// ============================================
// #define ENABLE_USER_AGENT_AUTH  true
// #define EXPECTED_USER_AGENT     "MVStech7689"

// ============================================
// HARDWARE PINS
// ============================================
#define LED_PIN       2
#define RELAY_PIN     4
#define SENSOR_PIN    34   // ADC input

// ============================================
// LIBRARY INSTANCES
// ============================================
MvsConnect device(DEVICE_NAME, FIRMWARE_VERSION);
MvsOTA mvsota;

// ============================================
// STATE VARIABLES
// ============================================
bool ledState = false;
bool relayState = false;
float sensorValue = 0.0;
unsigned long bootTime = 0;

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==========================================");
    Serial.println("  MvsConnect - Custom Dashboard");
    Serial.println("==========================================");

    // Setup GPIO
    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);

    bootTime = millis();

    // WiFi: AP + Station
    WiFi.mode(WIFI_AP_STA);
    String apName = String(DEVICE_NAME) + AP_SUFFIX;
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    Serial.printf("AP: %s (IP: %s)\n", apName.c_str(), WiFi.softAPIP().toString().c_str());

    // -----------------------------------------
    // Custom Dashboard Page (full HTML/CSS/JS)
    // -----------------------------------------
    device.setCustomHTML([]() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>" + device.getDeviceName() + " Dashboard</title>";
        html += "<style>";
        html += generateDashboardCSS();
        html += "</style></head><body>";

        // Header
        html += "<div class='header'>";
        html += "<h1>" + device.getDeviceName() + "</h1>";
        html += "<span>v" + device.getVersion() + " | Uptime: <span id='uptime'>0</span>s</span>";
        html += "</div>";

        // Status bar
        html += "<div class='status-bar'>";
        html += "<span>AP: " + WiFi.softAPIP().toString() + "</span>";
        if (WiFi.status() == WL_CONNECTED) {
            html += "<span>WiFi: " + WiFi.localIP().toString() + "</span>";
        }
        html += "<span>Heap: <span id='heap'>0</span> KB</span>";
        html += "</div>";

        html += "<div class='container'>";

        // Control Cards
        html += "<div class='card'>";
        html += "<h3>LED Control</h3>";
        html += "<div class='control-row'>";
        html += "<span id='led-status' class='state-badge " + String(ledState ? "on" : "off") + "'>";
        html += String(ledState ? "ON" : "OFF") + "</span>";
        html += "<button class='btn btn-toggle' onclick='toggle(\"led\")'>";
        html += "Toggle</button>";
        html += "</div></div>";

        html += "<div class='card'>";
        html += "<h3>Relay Control</h3>";
        html += "<div class='control-row'>";
        html += "<span id='relay-status' class='state-badge " + String(relayState ? "on" : "off") + "'>";
        html += String(relayState ? "ON" : "OFF") + "</span>";
        html += "<button class='btn btn-toggle' onclick='toggle(\"relay\")'>";
        html += "Toggle</button>";
        html += "</div></div>";

        // Sensor Card
        html += "<div class='card'>";
        html += "<h3>Sensor Reading</h3>";
        html += "<div class='sensor-value' id='sensor-val'>" + String(sensorValue, 1) + "</div>";
        html += "<div class='sensor-label'>Analog Value (GPIO " + String(SENSOR_PIN) + ")</div>";
        html += "</div>";

        // Device Info Card
        html += "<div class='card'>";
        html += "<h3>Device Info</h3>";
        html += "<div class='info-row'><span>Device</span><span>" + device.getDeviceName() + "</span></div>";
        html += "<div class='info-row'><span>Firmware</span><span>" + device.getVersion() + "</span></div>";
        html += "<div class='info-row'><span>WiFi</span><span id='wifi-status'>";
        html += String(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Not connected");
        html += "</span></div>";
        html += "</div>";

        html += "</div>"; // container

        // JavaScript
        html += "<script>";
        html += generateDashboardJS();
        html += "</script>";

        html += "</body></html>";
        return html;
    });

    // -----------------------------------------
    // REST API Endpoints
    // -----------------------------------------

    // Toggle LED
    device.addEndpoint("/api/toggle/led", []() {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        Serial.printf("LED: %s\n", ledState ? "ON" : "OFF");
        device.getServer()->send(200, "application/json",
            "{\"device\":\"led\",\"state\":" + String(ledState ? "true" : "false") + "}");
    });

    // Toggle Relay
    device.addEndpoint("/api/toggle/relay", []() {
        relayState = !relayState;
        digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
        Serial.printf("Relay: %s\n", relayState ? "ON" : "OFF");
        device.getServer()->send(200, "application/json",
            "{\"device\":\"relay\",\"state\":" + String(relayState ? "true" : "false") + "}");
    });

    // Get all status data (for auto-refresh)
    device.addEndpoint("/api/status", []() {
        String json = "{";
        json += "\"led\":" + String(ledState ? "true" : "false") + ",";
        json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
        json += "\"sensor\":" + String(sensorValue, 1) + ",";
        json += "\"uptime\":" + String((millis() - bootTime) / 1000) + ",";
        json += "\"heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
        json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
        json += "}";
        device.getServer()->send(200, "application/json", json);
    });

    // -----------------------------------------
    // Start Services
    // -----------------------------------------
    device.begin();
    mvsota.begin(DEVICE_NAME, FIRMWARE_VERSION);

    #ifdef ENABLE_USER_AGENT_AUTH
        #ifdef EXPECTED_USER_AGENT
            device.setUserAgentAuth(true, EXPECTED_USER_AGENT);
        #else
            device.setUserAgentAuth(true);
        #endif
    #endif

    device.onWiFiCredentialsReceived([](const String& ssid) {
        Serial.printf("Received WiFi credentials for: %s\n", ssid.c_str());
    });

    // Try saved WiFi
    if (device.connectToSavedWiFi(10000)) {
        Serial.printf("WiFi Connected: %s\n", WiFi.localIP().toString().c_str());
    }

    mvsota.onStart([]() {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(RELAY_PIN, LOW);
    });

    Serial.println("\n==========================================");
    Serial.println("  Setup Complete!");
    Serial.printf("  Dashboard: http://%s:%d/\n", WiFi.softAPIP().toString().c_str(), device.getPort());
    Serial.println("==========================================\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    device.handle();

    if (!mvsota.isUpdating()) {
        mvsota.handle();
    }

    // Read sensor every 2 seconds
    static unsigned long lastRead = 0;
    if (millis() - lastRead > 2000) {
        int raw = analogRead(SENSOR_PIN);
        sensorValue = (raw / 4095.0) * 100.0;  // Scale to 0-100
        lastRead = millis();
    }

    delay(10);
}

// ============================================
// DASHBOARD CSS (served as part of custom HTML)
// ============================================
String generateDashboardCSS() {
    return R"(
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, sans-serif; background: #1a1a2e; color: #eee;
               min-height: 100vh; }
        .header { background: linear-gradient(135deg, #667eea, #764ba2);
                  padding: 20px; text-align: center; }
        .header h1 { font-size: 22px; margin-bottom: 4px; }
        .header span { font-size: 13px; opacity: 0.8; }
        .status-bar { background: #0f3460; padding: 8px 16px; display: flex;
                      justify-content: space-around; font-size: 11px; opacity: 0.8; }
        .container { padding: 16px; max-width: 500px; margin: 0 auto; }
        .card { background: #16213e; border-radius: 12px; padding: 18px;
                margin-bottom: 14px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .card h3 { font-size: 15px; margin-bottom: 12px; opacity: 0.9; }
        .control-row { display: flex; justify-content: space-between; align-items: center; }
        .state-badge { padding: 6px 20px; border-radius: 20px; font-size: 14px; font-weight: bold; }
        .state-badge.on { background: #2d6a2d; color: #4ade80; }
        .state-badge.off { background: #394867; color: #888; }
        .btn { padding: 8px 20px; border: none; border-radius: 8px; cursor: pointer;
               font-size: 14px; font-weight: 600; }
        .btn-toggle { background: #667eea; color: white; }
        .btn-toggle:active { background: #5a6fd6; }
        .sensor-value { font-size: 48px; font-weight: bold; color: #667eea;
                        text-align: center; padding: 10px 0; }
        .sensor-label { text-align: center; font-size: 12px; opacity: 0.6; }
        .info-row { display: flex; justify-content: space-between; padding: 6px 0;
                    border-bottom: 1px solid #1a1a2e; font-size: 14px; }
        .info-row:last-child { border-bottom: none; }
    )";
}

// ============================================
// DASHBOARD JAVASCRIPT (auto-refresh)
// ============================================
String generateDashboardJS() {
    return R"(
        function toggle(dev) {
            fetch('/api/toggle/' + dev)
                .then(r => r.json())
                .then(d => updateUI(d))
                .catch(e => console.error(e));
        }

        function updateUI(data) {
            if (data.device === 'led' || data.led !== undefined) {
                var on = data.device ? data.state : data.led;
                var el = document.getElementById('led-status');
                el.textContent = on ? 'ON' : 'OFF';
                el.className = 'state-badge ' + (on ? 'on' : 'off');
            }
            if (data.device === 'relay' || data.relay !== undefined) {
                var on = data.device ? data.state : data.relay;
                var el = document.getElementById('relay-status');
                el.textContent = on ? 'ON' : 'OFF';
                el.className = 'state-badge ' + (on ? 'on' : 'off');
            }
        }

        function refresh() {
            fetch('/api/status')
                .then(r => r.json())
                .then(d => {
                    updateUI(d);
                    document.getElementById('sensor-val').textContent = d.sensor.toFixed(1);
                    document.getElementById('uptime').textContent = d.uptime;
                    document.getElementById('heap').textContent = d.heap;
                })
                .catch(e => {});
        }

        setInterval(refresh, 2000);
        refresh();
    )";
}
