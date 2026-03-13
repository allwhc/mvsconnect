/*
 * MvsConnect - Advanced Pin Control Example
 * ==========================================
 *
 * This example demonstrates:
 * - Multiple digital outputs (LEDs, relays)
 * - PWM outputs (LED brightness, motor speed)
 * - Digital inputs (buttons, sensors)
 * - Custom HTML sections
 * - Custom API endpoints
 * - WiFi credential transfer (built into MvsConnect)
 * - Integration with MvsOTA_ESP32
 *
 * LIBRARIES REQUIRED:
 * - MvsConnect (this library) - Web UI + WiFi credential transfer
 * - MvsOTA_ESP32 (optional) - OTA firmware updates
 *
 * HARDWARE EXAMPLE:
 * - GPIO 2:  Built-in LED (digital output)
 * - GPIO 4:  Relay module (digital output)
 * - GPIO 5:  External LED (PWM brightness)
 * - GPIO 18: Motor/Fan (PWM speed)
 * - GPIO 15: Push button (digital input)
 * - GPIO 34: Analog sensor (for custom display)
 */

#include <WiFi.h>
#include <mvsconnect.h>
#include <mvsota_esp32.h>

// ============================================
// DEVICE CONFIGURATION
// ============================================
#define DEVICE_NAME      "SmartController"
#define FIRMWARE_VERSION "1.2.0"

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================
// AP Suffix: MUST end with "_mvstech" for Android app discovery!
// The app scans for WiFi networks ending with "_mvstech"
#define AP_SUFFIX        "_mvstech"     // Required suffix for app discovery
#define AP_PASSWORD      "mvstech9867"  // Password for device WiFi (min 8 chars)

// ============================================
// WEB SERVER PORT CONFIGURATION
// ============================================
// Default port is 7689 (matches Android app default)
// To change: Uncomment and set your custom port
// IMPORTANT: Must match the port configured in Android app's ProfileManager!
// #define CUSTOM_WEB_PORT  7689        // Uncomment to use custom port

// ============================================
// USER-AGENT AUTHENTICATION (OPTIONAL)
// ============================================
// By default, device accepts ALL HTTP requests (no authentication).
// Enable User-Agent auth to only allow requests from the official app.
// When enabled, requests without correct User-Agent get 401 Unauthorized.
//
// To enable: Uncomment ENABLE_USER_AGENT_AUTH below
// To customize: Change EXPECTED_USER_AGENT (must match Android app)
//
// #define ENABLE_USER_AGENT_AUTH  true           // Uncomment to enable
// #define EXPECTED_USER_AGENT     "MVStech7689"  // Must match app's USER_AGENT

// ============================================
// PIN DEFINITIONS - CUSTOMIZE FOR YOUR HARDWARE
// ============================================

// Digital Outputs (ON/OFF control)
#define LED_BUILTIN_PIN  2      // Built-in LED
#define RELAY_PIN        4      // Relay for pump/light
#define INDICATOR_PIN    16     // Status indicator LED

// PWM Outputs (0-255 slider control)
#define LED_BRIGHTNESS_PIN  5   // External LED with brightness control
#define MOTOR_SPEED_PIN     18  // DC motor or fan speed

// Digital Inputs (read-only monitoring)
#define BUTTON_PIN       15     // Push button
#define DOOR_SENSOR_PIN  17     // Door/window sensor

// Analog Input (for custom display)
#define TEMP_SENSOR_PIN  34     // Temperature sensor (ADC)

// ============================================
// CREATE LIBRARY INSTANCES
// ============================================

// MvsConnect - handles web interface AND WiFi credential transfer
// Constructor: MvsConnect(deviceName, version, port)
//   - deviceName: Shown in app and used for mDNS discovery
//   - version: Firmware version string
//   - port: Web server port (default: 7689, must match Android app)
//
// Default port (7689):
MvsConnect device(DEVICE_NAME, FIRMWARE_VERSION);
//
// Custom port example (uncomment to use):
// MvsConnect device(DEVICE_NAME, FIRMWARE_VERSION, CUSTOM_WEB_PORT);

// MvsOTA - handles firmware updates on port 8089 (optional)
MvsOTA mvsota;

// ============================================
// NOTES ON ANDROID APP COMPATIBILITY
// ============================================
// 1. AP SSID must end with "_mvstech" for "Find Devices" to work
// 2. Web server port must match ProfileManager.getWebPort() in app
// 3. User-Agent authentication is OPTIONAL (disabled by default):
//    - App sends "MVStech7689" User-Agent with every request
//    - Enable via: device.setUserAgentAuth(true) or uncomment defines above
//    - When enabled, browser access will be blocked (401 Unauthorized)
// 4. For mDNS discovery on home WiFi, device name should contain
//    "mvstech" or it will be appended automatically

// ============================================
// GLOBAL VARIABLES
// ============================================

// Sensor data (updated in loop)
float temperature = 0.0;
int buttonPresses = 0;
unsigned long lastButtonPress = 0;

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==========================================");
    Serial.println("  MvsConnect - Advanced Pin Control");
    Serial.println("==========================================");

    // -----------------------------------------
    // WiFi Setup
    // -----------------------------------------
    WiFi.mode(WIFI_AP_STA);
    String apName = String(DEVICE_NAME) + AP_SUFFIX;
    WiFi.softAP(apName.c_str(), AP_PASSWORD);

    Serial.printf("AP: %s (IP: %s)\n", apName.c_str(), WiFi.softAPIP().toString().c_str());

    // -----------------------------------------
    // Register Digital Outputs
    // -----------------------------------------
    // These will appear as ON/OFF toggles in the web UI

    device.addDigitalOutput(LED_BUILTIN_PIN, "Status LED",
        "Built-in LED indicator", LOW);

    device.addDigitalOutput(RELAY_PIN, "Relay",
        "Controls pump or light", LOW);

    device.addDigitalOutput(INDICATOR_PIN, "Indicator",
        "External status LED", LOW);

    // -----------------------------------------
    // Register PWM Outputs
    // -----------------------------------------
    // These will appear as sliders (0-255) in the web UI

    device.addPWMOutput(LED_BRIGHTNESS_PIN, "Brightness",
        "LED brightness control", 0);

    device.addPWMOutput(MOTOR_SPEED_PIN, "Motor Speed",
        "Fan/motor PWM speed", 0);

    // -----------------------------------------
    // Register Digital Inputs
    // -----------------------------------------
    // These will show HIGH/LOW status in the web UI

    device.addDigitalInput(BUTTON_PIN, "Button",
        "User push button");

    device.addDigitalInput(DOOR_SENSOR_PIN, "Door Sensor",
        "Magnetic door/window sensor");

    // -----------------------------------------
    // Pin Change Callback
    // -----------------------------------------
    // Called when any output pin changes from web UI

    device.onPinChange([](int pin, int value) {
        Serial.printf("[Callback] Pin %d = %d\n", pin, value);

        // Example: When relay turns on, also turn on indicator
        if (pin == RELAY_PIN) {
            device.setPinValue(INDICATOR_PIN, value);
            Serial.printf("Relay %s - Indicator synced\n", value ? "ON" : "OFF");
        }

        // Example: Log motor speed changes
        if (pin == MOTOR_SPEED_PIN) {
            Serial.printf("Motor speed set to %d%%\n", (value * 100) / 255);
        }
    });

    // -----------------------------------------
    // Custom HTML Section
    // -----------------------------------------
    // Add your own content to the web page

    device.setCustomHTML([]() {
        String html = "<div class='card'>";
        html += "<div class='card-header'>";
        html += "<span class='card-title'>Sensor Data</span>";
        html += "</div>";

        // Temperature display
        html += "<div style='text-align:center;padding:10px;'>";
        html += "<div style='font-size:32px;color:#667eea;'>";
        html += String(temperature, 1) + " &deg;C";
        html += "</div>";
        html += "<div style='font-size:12px;opacity:0.6;'>Temperature</div>";
        html += "</div>";

        // Button press counter
        html += "<div style='text-align:center;padding:10px;'>";
        html += "<div style='font-size:24px;'>" + String(buttonPresses) + "</div>";
        html += "<div style='font-size:12px;opacity:0.6;'>Button Presses</div>";
        html += "</div>";

        html += "</div>";
        return html;
    });

    // -----------------------------------------
    // Custom API Endpoints
    // -----------------------------------------
    // Add your own REST API endpoints

    // GET /api/temperature - Returns temperature value
    device.addEndpoint("/api/temperature", []() {
        device.getServer()->send(200, "application/json",
            "{\"temperature\":" + String(temperature, 1) + "}");
    });

    // GET /api/sensor - Returns all sensor data
    device.addEndpoint("/api/sensor", []() {
        String json = "{";
        json += "\"temperature\":" + String(temperature, 1) + ",";
        json += "\"buttonPresses\":" + String(buttonPresses) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "}";
        device.getServer()->send(200, "application/json", json);
    });

    // GET /api/reset-counter - Reset button counter
    device.addEndpoint("/api/reset-counter", []() {
        buttonPresses = 0;
        device.getServer()->send(200, "text/plain", "Counter reset");
    });

    // -----------------------------------------
    // Start Services
    // -----------------------------------------
    device.begin();
    mvsota.begin(DEVICE_NAME, FIRMWARE_VERSION);

    // Optional: Enable User-Agent authentication (uncomment defines above)
    #ifdef ENABLE_USER_AGENT_AUTH
        #ifdef EXPECTED_USER_AGENT
            device.setUserAgentAuth(true, EXPECTED_USER_AGENT);
        #else
            device.setUserAgentAuth(true);  // Uses default "MVStech7689"
        #endif
    #endif

    // Optional: Callback when WiFi credentials received from app
    device.onWiFiCredentialsReceived([](const String& ssid) {
        Serial.printf("Received WiFi credentials for: %s\n", ssid.c_str());
    });

    // Try saved WiFi
    if (device.connectToSavedWiFi(10000)) {
        Serial.printf("WiFi Connected: %s\n", WiFi.localIP().toString().c_str());
    }

    // OTA callbacks
    mvsota.onStart([]() {
        // Turn off all outputs during OTA
        device.setPinValue(RELAY_PIN, LOW);
        device.setPinValue(MOTOR_SPEED_PIN, 0);
    });

    Serial.println("\n==========================================");
    Serial.println("  Setup Complete!");
    Serial.printf("  Web UI: http://%s/\n", WiFi.softAPIP().toString().c_str());
    Serial.println("==========================================\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Handle all services
    device.handle();
    if (!mvsota.isUpdating()) {
        mvsota.handle();
    }

    // -----------------------------------------
    // Read Temperature Sensor (every 2 seconds)
    // -----------------------------------------
    static unsigned long lastTempRead = 0;
    if (millis() - lastTempRead > 2000) {
        // Read analog value and convert to temperature
        // Adjust formula for your specific sensor!
        int rawValue = analogRead(TEMP_SENSOR_PIN);
        temperature = (rawValue / 4095.0) * 100.0;  // Example: 0-100 range
        lastTempRead = millis();
    }

    // -----------------------------------------
    // Handle Button Press (with debounce)
    // -----------------------------------------
    static bool lastButtonState = HIGH;
    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState == LOW && lastButtonState == HIGH) {
        // Button pressed (assuming active LOW)
        if (millis() - lastButtonPress > 200) {  // 200ms debounce
            buttonPresses++;
            lastButtonPress = millis();
            Serial.printf("Button pressed! Count: %d\n", buttonPresses);

            // Example: Toggle relay on button press
            device.togglePin(RELAY_PIN);
        }
    }
    lastButtonState = currentButtonState;

    // -----------------------------------------
    // Your Custom Logic Here
    // -----------------------------------------
    // Example: Auto-control based on temperature
    //
    // if (temperature > 30.0) {
    //     device.setPinValue(MOTOR_SPEED_PIN, 255);  // Fan full speed
    // } else if (temperature < 25.0) {
    //     device.setPinValue(MOTOR_SPEED_PIN, 0);    // Fan off
    // }

    delay(10);
}

// ============================================
// HELPER FUNCTIONS
// ============================================

// Example: Function to set all outputs off
void allOutputsOff() {
    device.setPinValue(LED_BUILTIN_PIN, LOW);
    device.setPinValue(RELAY_PIN, LOW);
    device.setPinValue(INDICATOR_PIN, LOW);
    device.setPinValue(LED_BRIGHTNESS_PIN, 0);
    device.setPinValue(MOTOR_SPEED_PIN, 0);
}

// Example: Function to run startup animation
void startupAnimation() {
    for (int i = 0; i < 3; i++) {
        device.setPinValue(LED_BUILTIN_PIN, HIGH);
        delay(100);
        device.setPinValue(LED_BUILTIN_PIN, LOW);
        delay(100);
    }
}
