/*
 * MvsConnect - Basic LED Control Example
 * =======================================
 *
 * This example shows how to:
 * 1. Control an LED from Android app (mvsConnect)
 * 2. Transfer WiFi credentials from phone to ESP32 (built-in!)
 * 3. Enable over-the-air firmware updates
 *
 * LIBRARIES REQUIRED:
 * - MvsConnect (this library) - Web UI + WiFi credential transfer
 * - MvsOTA_ESP32 - Over-the-air firmware updates (optional)
 *
 * HARDWARE:
 * - ESP32 board
 * - LED on GPIO 2 (built-in on most boards)
 *
 * HOW TO USE:
 * 1. Upload this sketch to ESP32
 * 2. Install mvsConnect Android app
 * 3. In app: Click "Connect to My Device" or scan QR
 * 4. Control the LED from the web interface
 * 5. Use "Transfer WiFi" to send your home WiFi to ESP32
 * 6. Use "Firmware Update" to update OTA
 */

#include <WiFi.h>

// ============================================
// INCLUDE LIBRARIES
// ============================================
#include <mvsconnect.h>      // Web UI + WiFi credential transfer (built-in)
#include <mvsota_esp32.h>    // OTA firmware updates (optional)

// ============================================
// DEVICE CONFIGURATION - CUSTOMIZE THESE!
// ============================================
#define DEVICE_NAME     "MyESP32"       // Your device name (shown in app)
#define FIRMWARE_VERSION "1.0.0"        // Update this for OTA tracking

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================
// AP Suffix: MUST end with "_mvstech" for Android app discovery!
// The app scans for WiFi networks ending with "_mvstech"
#define AP_SUFFIX       "_mvstech"      // Required suffix for app discovery
#define AP_PASSWORD     "mvstech9867"   // Password for device WiFi (min 8 chars)

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
// PIN DEFINITIONS - CUSTOMIZE THESE!
// ============================================
#define LED_PIN         2               // Built-in LED (GPIO 2 on most ESP32)
// Add more pins as needed:
// #define RELAY_PIN    4
// #define BUZZER_PIN   5

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
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  MvsConnect - Basic LED Example");
    Serial.println("========================================");
    Serial.printf("  Device: %s\n", DEVICE_NAME);
    Serial.printf("  Version: %s\n", FIRMWARE_VERSION);
    Serial.println("========================================\n");

    // -----------------------------------------
    // STEP 1: Configure WiFi (AP + Station mode)
    // -----------------------------------------
    // AP mode: For Android app to connect directly
    // STA mode: To connect to your home WiFi later
    WiFi.mode(WIFI_AP_STA);

    // Start Access Point with _mvstech suffix (REQUIRED for app discovery!)
    String apName = String(DEVICE_NAME) + AP_SUFFIX;
    WiFi.softAP(apName.c_str(), AP_PASSWORD);

    Serial.printf("Access Point Started!\n");
    Serial.printf("  SSID: %s\n", apName.c_str());
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());

    // -----------------------------------------
    // STEP 2: Register pins for web control
    // -----------------------------------------
    // Add your pins BEFORE calling device.begin()

    // Add LED as digital output (ON/OFF toggle in UI)
    device.addDigitalOutput(LED_PIN, "LED", "Built-in blue LED", LOW);

    // *** ADD YOUR OWN PINS HERE ***
    // Examples:
    // device.addDigitalOutput(4, "Relay", "Water pump control", LOW);
    // device.addDigitalOutput(5, "Buzzer", "Alert buzzer", LOW);
    // device.addPWMOutput(18, "Brightness", "LED brightness 0-255", 0);
    // device.addDigitalInput(15, "Button", "User button");

    // Optional: Callback when pin changes from web UI
    device.onPinChange([](int pin, int value) {
        Serial.printf("Pin %d changed to %d\n", pin, value);

        // *** ADD YOUR CUSTOM LOGIC HERE ***
        // Example: When LED turns on, do something
        if (pin == LED_PIN && value == HIGH) {
            Serial.println("LED is now ON!");
            // Add your code here...
        }
    });

    // -----------------------------------------
    // STEP 3: Start MvsConnect web server
    // -----------------------------------------
    // MvsConnect handles both web UI AND WiFi credential transfer!
    device.begin();

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

    // Try to connect to previously saved WiFi
    if (device.connectToSavedWiFi(10000)) {
        Serial.printf("Connected to saved WiFi!\n");
        Serial.printf("  WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("No saved WiFi - waiting for credentials from app");
    }

    // -----------------------------------------
    // STEP 4: Start OTA update service (optional)
    // -----------------------------------------
    // This allows Android app to update firmware
    mvsota.begin(DEVICE_NAME, FIRMWARE_VERSION);

    // Optional: OTA callbacks
    mvsota.onStart([]() {
        Serial.println("OTA Update Starting...");
        // Turn off outputs during update for safety
        digitalWrite(LED_PIN, LOW);
    });

    mvsota.onProgress([](int percent, size_t current, size_t total) {
        // Blink LED during update
        digitalWrite(LED_PIN, (percent / 5) % 2);
    });

    mvsota.onEnd([](bool success) {
        Serial.printf("OTA %s!\n", success ? "Successful" : "Failed");
    });

    // -----------------------------------------
    // DONE! Print summary
    // -----------------------------------------
    Serial.println("\n========================================");
    Serial.println("  Setup Complete! Available Services:");
    Serial.println("========================================");
    Serial.printf("  Web UI + WiFi: http://%s:%d/\n", WiFi.softAPIP().toString().c_str(), device.getPort());
    Serial.printf("  OTA Update:    Port 8089\n");
    Serial.println("========================================");
    Serial.println("  Open mvsConnect Android app and");
    Serial.println("  tap 'Connect to My Device' to start!");
    Serial.println("========================================\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Handle web interface and WiFi credential requests
    device.handle();

    // Handle OTA update requests (skip if already updating)
    if (!mvsota.isUpdating()) {
        mvsota.handle();
    }

    // *** ADD YOUR OWN CODE HERE ***
    // Example: Read sensor, update display, etc.
    //
    // if (millis() - lastRead > 1000) {
    //     float temp = readTemperature();
    //     Serial.printf("Temperature: %.1f C\n", temp);
    //     lastRead = millis();
    // }

    delay(10);
}

// ============================================
// *** ADD YOUR CUSTOM FUNCTIONS HERE ***
// ============================================
// Example:
//
// float readTemperature() {
//     // Read from sensor
//     return 25.5;
// }
//
// void controlMotor(int speed) {
//     device.setPinValue(MOTOR_PIN, speed);
// }
