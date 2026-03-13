/*
 * MvsConnect - ESP32 IoT Web Server Library Implementation
 * Version: 1.2.2
 */

#include "MvsConnect.h"

// ============================================
// STATIC CONSTANTS
// ============================================

const char* MvsConnect::NVS_NAMESPACE = "mvswifi";
const char* MvsConnect::KEY_SSID = "ssid";
const char* MvsConnect::KEY_PASSWORD = "pass";
const char* MvsConnect::KEY_VALID = "valid";

// ============================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================

MvsConnect::MvsConnect(const char* deviceName, const char* version, int port) {
    _deviceName = String(deviceName);
    _version = String(version);
    _port = port;
    _server = nullptr;
    _running = false;
    _debug = true;
    _customHTMLCallback = nullptr;
    _wifiCallback = nullptr;
    _isConnecting = false;
    _wifiConnectStart = 0;
    _wifiStatus = "READY";
    _userAgentAuthEnabled = false;
    _expectedUserAgent = MVSCONNECT_DEFAULT_USER_AGENT;
}

MvsConnect::~MvsConnect() {
    if (_server) {
        _server->stop();
        delete _server;
    }
}

// ============================================
// MAIN METHODS
// ============================================

void MvsConnect::begin() {
    if (_running) return;

    log("MvsConnect v1.2.2 starting...");
    log("Device: " + _deviceName + " (v" + _version + ")");

    // Create web server
    _server = new WebServer(_port);

    // Setup mDNS for Android app discovery
    setupMDNS();

    // Setup web routes
    setupRoutes();

    // Start server
    _server->begin();
    _running = true;

    log("Web server started on port " + String(_port));
    log("Access via: http://" + WiFi.softAPIP().toString() + ":" + String(_port));
    log("WiFi transfer endpoint: /__mvsconnect/wifi/transfer");

    if (WiFi.status() == WL_CONNECTED) {
        log("Also available at: http://" + WiFi.localIP().toString() + ":" + String(_port));
    }
}

void MvsConnect::handle() {
    if (!_running || !_server) return;

    _server->handleClient();

    // Update WiFi connection status
    updateWiFiConnectionStatus();
}

// ============================================
// WIFI CREDENTIAL METHODS
// ============================================

bool MvsConnect::connectToSavedWiFi(unsigned long timeout) {
    if (!hasSavedWiFi()) {
        log("No saved WiFi credentials in NVS");
        return false;
    }

    String ssid = readSavedSSID();
    String password = readSavedPassword();

    if (ssid.length() == 0) {
        log("Invalid saved credentials");
        return false;
    }

    log("Connecting to saved WiFi: " + ssid);

    _lastSSID = ssid;
    _isConnecting = true;
    _wifiConnectStart = millis();
    _wifiStatus = "CONNECTING: " + ssid;

    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
        delay(100);
    }

    _isConnecting = false;

    if (WiFi.status() == WL_CONNECTED) {
        _wifiStatus = "CONNECTED: " + ssid + " IP:" + WiFi.localIP().toString();
        log("Connected to WiFi! IP: " + WiFi.localIP().toString());

        // Restart mDNS to advertise on WiFi network
        setupMDNS();

        return true;
    }

    _wifiStatus = "FAILED: Connection timeout to " + ssid;
    log("WiFi connection failed");
    return false;
}

bool MvsConnect::hasSavedWiFi() {
    _preferences.begin(NVS_NAMESPACE, true);
    bool valid = _preferences.getBool(KEY_VALID, false);
    _preferences.end();
    return valid;
}

void MvsConnect::clearSavedWiFi() {
    _preferences.begin(NVS_NAMESPACE, false);
    _preferences.clear();
    _preferences.end();
    log("WiFi credentials cleared from NVS");
}

String MvsConnect::getWiFiStatus() {
    updateWiFiConnectionStatus();
    return _wifiStatus;
}

bool MvsConnect::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void MvsConnect::saveWiFiCredentials(const String& ssid, const String& password) {
    _preferences.begin(NVS_NAMESPACE, false);
    _preferences.putString(KEY_SSID, ssid);
    _preferences.putString(KEY_PASSWORD, password);
    _preferences.putBool(KEY_VALID, true);
    _preferences.end();
    log("WiFi credentials saved to NVS for: " + ssid);
}

String MvsConnect::readSavedSSID() {
    _preferences.begin(NVS_NAMESPACE, true);
    String ssid = _preferences.getString(KEY_SSID, "");
    _preferences.end();
    return ssid;
}

String MvsConnect::readSavedPassword() {
    _preferences.begin(NVS_NAMESPACE, true);
    String password = _preferences.getString(KEY_PASSWORD, "");
    _preferences.end();
    return password;
}

void MvsConnect::updateWiFiConnectionStatus() {
    if (!_isConnecting && WiFi.status() != WL_CONNECTED) {
        if (_lastSSID.length() > 0) {
            _wifiStatus = "FAILED: Cannot connect to " + _lastSSID;
        } else {
            _wifiStatus = "READY: No WiFi configured";
        }
        return;
    }

    if (_isConnecting) {
        unsigned long elapsed = (millis() - _wifiConnectStart) / 1000;

        if (WiFi.status() == WL_CONNECTED) {
            _wifiStatus = "CONNECTED: " + _lastSSID + " IP:" + WiFi.localIP().toString();
            _isConnecting = false;
            log("Connected to " + _lastSSID + " (IP: " + WiFi.localIP().toString() + ")");

            // Restart mDNS to advertise on WiFi network
            setupMDNS();
        } else if (elapsed > (MVSCONNECT_WIFI_TIMEOUT / 1000)) {
            _wifiStatus = "FAILED: Connection timeout to " + _lastSSID;
            _isConnecting = false;
            log("Connection failed: " + _lastSSID);
        } else {
            _wifiStatus = "CONNECTING: " + _lastSSID + " (" + String(elapsed) + "s)";
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        _wifiStatus = "CONNECTED: " + WiFi.SSID() + " IP:" + WiFi.localIP().toString();
    }
}

// ============================================
// CALLBACKS
// ============================================

void MvsConnect::setCustomHTML(MvsConnectCustomHTMLCallback callback) {
    _customHTMLCallback = callback;
}

void MvsConnect::onWiFiCredentialsReceived(MvsConnectWiFiCallback callback) {
    _wifiCallback = callback;
}

// ============================================
// CUSTOM ENDPOINTS
// ============================================

void MvsConnect::addEndpoint(const char* path, WebServer::THandlerFunction handler) {
    if (_server) {
        _server->on(path, handler);
        log("Added endpoint: " + String(path));
    }
}

WebServer* MvsConnect::getServer() {
    return _server;
}

// ============================================
// STATUS METHODS
// ============================================

String MvsConnect::getDeviceName() { return _deviceName; }
String MvsConnect::getVersion() { return _version; }
int MvsConnect::getPort() { return _port; }
bool MvsConnect::isRunning() { return _running; }

void MvsConnect::setDebug(bool enabled) {
    _debug = enabled;
}

// ============================================
// USER-AGENT AUTHENTICATION
// ============================================

void MvsConnect::setUserAgentAuth(bool enabled, const char* userAgent) {
    _userAgentAuthEnabled = enabled;
    _expectedUserAgent = String(userAgent);

    if (enabled) {
        log("User-Agent authentication ENABLED. Expected: " + _expectedUserAgent);
    } else {
        log("User-Agent authentication DISABLED (accepting all requests)");
    }
}

bool MvsConnect::isUserAgentAuthEnabled() {
    return _userAgentAuthEnabled;
}

String MvsConnect::getExpectedUserAgent() {
    return _expectedUserAgent;
}

bool MvsConnect::checkUserAgent() {
    if (!_userAgentAuthEnabled) {
        return true;
    }

    if (_server->hasHeader("User-Agent")) {
        String userAgent = _server->header("User-Agent");
        if (userAgent == _expectedUserAgent) {
            return true;
        }
        log("Unauthorized: Wrong User-Agent: " + userAgent);
    } else {
        log("Unauthorized: Missing User-Agent header");
    }

    _server->send(401, "text/plain", "Unauthorized: Invalid User-Agent");
    return false;
}

// ============================================
// INTERNAL SETUP
// ============================================

void MvsConnect::setupMDNS() {
    if (WiFi.status() != WL_CONNECTED) {
        log("mDNS skipped: Not connected to WiFi network");
        return;
    }

    MDNS.end();

    String mdnsName = _deviceName;
    mdnsName.replace(" ", "-");
    mdnsName.toLowerCase();

    if (mdnsName.indexOf("mvstech") < 0) {
        mdnsName += "-mvstech";
    }

    if (MDNS.begin(mdnsName.c_str())) {
        MDNS.addService("http", "tcp", _port);
        MDNS.addServiceTxt("http", "tcp", "device", _deviceName.c_str());
        MDNS.addServiceTxt("http", "tcp", "version", _version.c_str());
        MDNS.addService("mvstech", "tcp", _port);

        log("mDNS started: " + mdnsName + ".local (" + WiFi.localIP().toString() + ")");
    } else {
        log("mDNS failed to start");
    }
}

void MvsConnect::setupRoutes() {
    // Collect User-Agent header for authentication
    const char* headerKeys[] = {"User-Agent"};
    _server->collectHeaders(headerKeys, 1);

    // 404 handler - registered FIRST so all specific routes override it
    _server->onNotFound([this]() {
        _server->send(404, "text/plain", "Not Found");
    });

    // Main page
    _server->on("/", HTTP_GET, [this]() { handleRoot(); });

    // Device info API
    _server->on("/info", HTTP_GET, [this]() { handleGetInfo(); });

    // WiFi transfer endpoints
    _server->on("/__mvsconnect/wifi/transfer", HTTP_ANY, [this]() { handleWiFiTransfer(); });
    _server->on("/__mvsconnect/wifi/status", HTTP_ANY, [this]() { handleWiFiStatus(); });
}

// ============================================
// ROUTE HANDLERS
// ============================================

void MvsConnect::handleRoot() {
    if (!checkUserAgent()) return;

    if (_customHTMLCallback) {
        _server->send(200, "text/html", _customHTMLCallback());
    } else {
        // Default minimal page when no custom HTML is set
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>" + _deviceName + "</title>";
        html += "<style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px;}</style>";
        html += "</head><body>";
        html += "<h1>" + _deviceName + "</h1>";
        html += "<p>v" + _version + "</p>";
        html += "<p>Use setCustomHTML() to serve your web app.</p>";
        html += "</body></html>";
        _server->send(200, "text/html", html);
    }
}

void MvsConnect::handleGetInfo() {
    if (!checkUserAgent()) return;

    String json = "{";
    json += "\"device\":\"" + _deviceName + "\",";
    json += "\"version\":\"" + _version + "\",";
    json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    if (WiFi.status() == WL_CONNECTED) {
        json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\",";
    }
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "}";

    _server->send(200, "application/json", json);
}

// ============================================
// ROUTE HANDLERS - WIFI TRANSFER
// ============================================

void MvsConnect::handleWiFiTransfer() {
    if (!checkUserAgent()) return;

    if (!_server->hasArg("ssid")) {
        _server->send(400, "text/plain", "Usage: /__mvsconnect/wifi/transfer?ssid=YourWiFi&password=YourPass");
        return;
    }

    String ssid = _server->arg("ssid");
    String password = _server->hasArg("password") ? _server->arg("password") : "";

    log("WiFi credentials received for: " + ssid);

    // Save credentials to NVS
    saveWiFiCredentials(ssid, password);

    // Start connection
    _lastSSID = ssid;
    _isConnecting = true;
    _wifiConnectStart = millis();
    _wifiStatus = "CONNECTING: " + ssid;

    WiFi.begin(ssid.c_str(), password.c_str());

    // Call user callback
    if (_wifiCallback) {
        _wifiCallback(ssid);
    }

    // Send immediate response
    String response = "OK: Connecting to " + ssid + "\n";
    response += "Status: http://" + WiFi.softAPIP().toString() + ":" + String(_port) + "/__mvsconnect/wifi/status";

    _server->send(200, "text/plain", response);
}

void MvsConnect::handleWiFiStatus() {
    if (!checkUserAgent()) return;

    updateWiFiConnectionStatus();
    _server->send(200, "text/plain", _wifiStatus);
}

// ============================================
// HELPERS
// ============================================

void MvsConnect::log(const String& message) {
    if (_debug) {
        Serial.println("[MvsConnect] " + message);
    }
}
