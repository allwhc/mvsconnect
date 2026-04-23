/*
 * MvsConnect - ESP32 IoT Web Server Library
 * ==========================================
 * Version: 1.2.2
 * Platform: ESP32 only
 *
 * Lightweight library for ESP32 IoT devices with Android app support.
 * Handles custom web pages, WiFi credential transfer, and mDNS discovery.
 *
 * Features:
 * - Built-in web server with custom HTML page support
 * - WiFi credential transfer from Android app
 * - mDNS service discovery for Android app
 * - Custom REST API endpoints
 * - User-Agent authentication (optional)
 *
 * Optional companion library:
 * - MvsOTA_ESP32: For over-the-air firmware updates (separate port)
 *
 * Usage:
 *   #include <MvsConnect.h>
 *   #include <mvsota_esp32.h>   // Optional: OTA updates
 *
 *   MvsConnect device("MyDevice", "1.0.0");
 *   MvsOTA mvsota;  // Optional
 *
 *   void setup() {
 *     WiFi.mode(WIFI_AP_STA);
 *     WiFi.softAP("MyDevice_mvstech", "password");
 *
 *     device.setCustomHTML([]() { return String("<!DOCTYPE html>..."); });
 *     device.addEndpoint("/api/data", handler);
 *     device.begin();
 *
 *     device.connectToSavedWiFi(10000);
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

typedef String (*MvsConnectCustomHTMLCallback)();
typedef void (*MvsConnectWiFiCallback)(const String& ssid);

// ============================================
// MAIN CLASS
// ============================================

class MvsConnect {
public:
    /**
     * Constructor
     * @param deviceName Name of your device (shown in mDNS)
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
     * Override the device name (used for mDNS) before calling begin().
     * Useful when the name depends on runtime data like the device code.
     */
    void setDeviceName(const String& name);

    /**
     * Handle web requests - call in loop()
     */
    void handle();

    // ============================================
    // WIFI CREDENTIAL METHODS
    // ============================================

    /**
     * Connect to saved WiFi credentials (from NVS)
     * @param timeout Connection timeout in milliseconds
     * @return true if connected successfully
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
     * Set custom HTML page served at "/"
     * Callback must return a complete HTML page (<!DOCTYPE html>...)
     */
    void setCustomHTML(MvsConnectCustomHTMLCallback callback);

    /**
     * Set callback for when WiFi credentials are received from Android app
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
     *
     * @param enabled true to enable, false to disable (default: false)
     * @param userAgent Expected User-Agent string (default: "MVStech7689")
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

    // Callbacks
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

    // mDNS state — tracked so handle() can (re)start it whenever WiFi is up
    bool _mdnsStarted;
    unsigned long _lastMdnsCheckMs;

    // Internal methods
    bool checkUserAgent();
    void setupMDNS();
    void setupRoutes();

    // Route handlers
    void handleRoot();
    void handleGetInfo();
    void handleWiFiTransfer();
    void handleWiFiStatus();

    // NVS helpers
    void saveWiFiCredentials(const String& ssid, const String& password);
    String readSavedSSID();
    String readSavedPassword();
    void updateWiFiConnectionStatus();

    // Helpers
    void log(const String& message);
};

#endif // MVSCONNECT_H
