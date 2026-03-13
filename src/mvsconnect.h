/*
 * MvsConnect - ESP32 IoT Web Server Library
 * ==========================================
 * Version: 1.2.1
 * Platform: ESP32 only
 *
 * All-in-one library for ESP32 IoT devices with Android app support.
 * Handles web UI, pin control, and WiFi credential transfer.
 *
 * Features:
 * - Built-in web server for device control
 * - Easy pin registration (digital/PWM)
 * - Mobile-friendly responsive UI
 * - WiFi credential transfer from Android app (built-in)
 * - mDNS service discovery for Android app
 * - Customizable web pages
 * - Callback system for pin changes
 *
 * Optional companion library:
 * - MvsOTA_ESP32: For over-the-air firmware updates (separate port)
 *
 * Usage:
 *   #include <mvsconnect.h>
 *   #include <mvsota_esp32.h>   // Optional: OTA updates
 *
 *   MvsConnect device("MyDevice", "1.0.0");
 *   MvsOTA mvsota;  // Optional
 *
 *   void setup() {
 *     WiFi.mode(WIFI_AP_STA);
 *     WiFi.softAP("MyDevice_mvstech", "password");
 *
 *     device.addDigitalOutput(LED_BUILTIN, "LED");
 *     device.begin();
 *
 *     // Try to connect to saved WiFi
 *     device.connectToSavedWiFi(10000);
 *
 *     mvsota.begin("MyDevice", "1.0.0");  // Optional
 *   }
 *
 *   void loop() {
 *     device.handle();
 *     mvsota.handle();  // Optional
 *   }
 *
 * Author: MV Solutions
 * License: MIT
 */

#ifndef MVSCONNECT_H
#define MVSCONNECT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ============================================
// DEFAULT CONFIGURATION
// ============================================

#define MVSCONNECT_DEFAULT_PORT  7689  // Default web server port (matches Android app)
#define MVSCONNECT_WIFI_TIMEOUT  30000 // WiFi connection timeout (ms)
#define MVSCONNECT_DEFAULT_USER_AGENT "MVStech7689"  // Default User-Agent (matches Android app)

// ============================================
// CALLBACK TYPES
// ============================================

typedef void (*MvsConnectPinCallback)(int pin, int value);
typedef String (*MvsConnectCustomHTMLCallback)();
typedef void (*MvsConnectWiFiCallback)(const String& ssid);

// ============================================
// PIN CONTROL STRUCTURE
// ============================================

struct MvsPin {
    int pin;
    String name;
    String description;
    bool isOutput;
    bool isPWM;
    int value;
    int pwmChannel;
    int minValue;
    int maxValue;
};

// ============================================
// MAIN CLASS
// ============================================

class MvsConnect {
public:
    /**
     * Constructor
     * @param deviceName Name of your device (shown in UI and mDNS)
     * @param version Firmware version string
     * @param port Web server port (default: 7689)
     */
    MvsConnect(const char* deviceName, const char* version, int port = MVSCONNECT_DEFAULT_PORT);

    /**
     * Destructor
     */
    ~MvsConnect();

    /**
     * Initialize web server and mDNS - call in setup() after WiFi.softAP()
     */
    void begin();

    /**
     * Handle web requests - call in loop()
     */
    void handle();

    // ============================================
    // PIN REGISTRATION METHODS
    // ============================================

    /**
     * Add a digital output pin (ON/OFF control)
     * @param pin GPIO pin number
     * @param name Display name (e.g., "LED", "Relay")
     * @param description Optional description shown in UI
     * @param initialValue Initial state (LOW or HIGH)
     */
    void addDigitalOutput(int pin, const char* name, const char* description = "", int initialValue = LOW);

    /**
     * Add a PWM output pin (slider control 0-255)
     * @param pin GPIO pin number
     * @param name Display name (e.g., "Brightness", "Speed")
     * @param description Optional description
     * @param initialValue Initial PWM value (0-255)
     */
    void addPWMOutput(int pin, const char* name, const char* description = "", int initialValue = 0);

    /**
     * Add a digital input pin (read-only monitoring)
     * @param pin GPIO pin number
     * @param name Display name (e.g., "Button", "Sensor")
     * @param description Optional description
     */
    void addDigitalInput(int pin, const char* name, const char* description = "");

    // ============================================
    // PIN CONTROL METHODS
    // ============================================

    /**
     * Set pin value programmatically (also updates web UI)
     */
    void setPinValue(int pin, int value);

    /**
     * Get current pin value
     */
    int getPinValue(int pin);

    /**
     * Toggle digital output pin
     */
    int togglePin(int pin);

    // ============================================
    // WIFI CREDENTIAL METHODS
    // ============================================

    /**
     * Connect to saved WiFi credentials (from NVS)
     * @param timeout Connection timeout in milliseconds
     * @return true if connected successfully
     *
     * Example:
     *   if (device.connectToSavedWiFi(10000)) {
     *       Serial.println("Connected to WiFi!");
     *   } else {
     *       Serial.println("No saved WiFi or connection failed");
     *   }
     */
    bool connectToSavedWiFi(unsigned long timeout = 10000);

    /**
     * Check if WiFi credentials are saved in NVS
     */
    bool hasSavedWiFi();

    /**
     * Clear saved WiFi credentials from NVS
     */
    void clearSavedWiFi();

    /**
     * Get current WiFi connection status string
     * Returns: "CONNECTING: SSID (Xs)", "CONNECTED: SSID IP:x.x.x.x", "FAILED: reason", etc.
     */
    String getWiFiStatus();

    /**
     * Check if currently connected to WiFi
     */
    bool isWiFiConnected();

    // ============================================
    // CALLBACK METHODS
    // ============================================

    /**
     * Set callback for when pin value changes (from web UI)
     */
    void onPinChange(MvsConnectPinCallback callback);

    /**
     * Add custom HTML content to the main page
     */
    void setCustomHTML(MvsConnectCustomHTMLCallback callback);

    /**
     * Set callback for when WiFi credentials are received from Android app
     *
     * Example:
     *   device.onWiFiCredentialsReceived([](const String& ssid) {
     *       Serial.println("Received WiFi credentials for: " + ssid);
     *   });
     */
    void onWiFiCredentialsReceived(MvsConnectWiFiCallback callback);

    // ============================================
    // CUSTOM ENDPOINT METHODS
    // ============================================

    /**
     * Add custom endpoint to web server
     */
    void addEndpoint(const char* path, WebServer::THandlerFunction handler);

    /**
     * Get WebServer instance for advanced customization
     */
    WebServer* getServer();

    // ============================================
    // STATUS METHODS
    // ============================================

    String getDeviceName();
    String getVersion();
    int getPort();
    int getPinCount();
    bool isRunning();

    /**
     * Enable/disable Serial debug output
     */
    void setDebug(bool enabled);

    // ============================================
    // USER-AGENT AUTHENTICATION (OPTIONAL)
    // ============================================

    /**
     * Enable User-Agent authentication
     * When enabled, only requests with matching User-Agent header are allowed.
     * Requests without correct User-Agent will receive 401 Unauthorized.
     *
     * @param enabled true to enable authentication, false to disable (default: false)
     * @param userAgent Expected User-Agent string (default: "MVStech7689")
     *
     * Example:
     *   // Enable with default User-Agent (MVStech7689)
     *   device.setUserAgentAuth(true);
     *
     *   // Enable with custom User-Agent
     *   device.setUserAgentAuth(true, "MyCustomAgent123");
     *
     *   // Disable authentication (accept all requests)
     *   device.setUserAgentAuth(false);
     */
    void setUserAgentAuth(bool enabled, const char* userAgent = MVSCONNECT_DEFAULT_USER_AGENT);

    /**
     * Check if User-Agent authentication is enabled
     */
    bool isUserAgentAuthEnabled();

    /**
     * Get the expected User-Agent string
     */
    String getExpectedUserAgent();

private:
    String _deviceName;
    String _version;
    int _port;
    WebServer* _server;
    bool _running;
    bool _debug;

    // Pin management
    static const int MAX_PINS = 20;
    MvsPin _pins[MAX_PINS];
    int _pinCount;
    int _nextPwmChannel;

    // Callbacks
    MvsConnectPinCallback _pinCallback;
    MvsConnectCustomHTMLCallback _customHTMLCallback;
    MvsConnectWiFiCallback _wifiCallback;

    // NVS for WiFi credentials
    Preferences _preferences;
    static const char* NVS_NAMESPACE;
    static const char* KEY_SSID;
    static const char* KEY_PASSWORD;
    static const char* KEY_VALID;

    // WiFi connection tracking
    String _lastSSID;
    String _wifiStatus;
    unsigned long _wifiConnectStart;
    bool _isConnecting;

    // User-Agent authentication
    bool _userAgentAuthEnabled;
    String _expectedUserAgent;

    // Internal methods
    bool checkUserAgent();  // Returns true if request is authorized
    void setupMDNS();
    void setupRoutes();

    // Route handlers - Pin control
    void handleRoot();
    void handleSetPin();
    void handleGetPins();
    void handleGetInfo();

    // Route handlers - WiFi transfer
    void handleWiFiTransfer();
    void handleWiFiStatus();

    // HTML generation
    String generateHTML();
    String generateCSS();
    String generateJS();
    String generatePinCards();

    // NVS helpers
    void saveWiFiCredentials(const String& ssid, const String& password);
    String readSavedSSID();
    String readSavedPassword();
    void updateWiFiConnectionStatus();

    // Helpers
    MvsPin* findPin(int pin);
    void log(const String& message);
};

#endif // MVSCONNECT_H
