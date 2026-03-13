/*
 * MvsConnect - ESP32 IoT Web Server Library Implementation
 * Version: 1.2.1
 */

#include "mvsconnect.h"

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
    _pinCount = 0;
    _nextPwmChannel = 0;
    _pinCallback = nullptr;
    _customHTMLCallback = nullptr;
    _wifiCallback = nullptr;
    _isConnecting = false;
    _wifiConnectStart = 0;
    _wifiStatus = "READY";
    _userAgentAuthEnabled = false;  // Disabled by default
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

    log("MvsConnect v1.2.1 starting...");
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

    // Update input pin values
    for (int i = 0; i < _pinCount; i++) {
        if (!_pins[i].isOutput) {
            _pins[i].value = digitalRead(_pins[i].pin);
        }
    }

    // Update WiFi connection status
    updateWiFiConnectionStatus();
}

// ============================================
// PIN REGISTRATION
// ============================================

void MvsConnect::addDigitalOutput(int pin, const char* name, const char* description, int initialValue) {
    if (_pinCount >= MAX_PINS) {
        log("ERROR: Maximum pins reached!");
        return;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, initialValue);

    _pins[_pinCount].pin = pin;
    _pins[_pinCount].name = String(name);
    _pins[_pinCount].description = String(description);
    _pins[_pinCount].isOutput = true;
    _pins[_pinCount].isPWM = false;
    _pins[_pinCount].value = initialValue;
    _pins[_pinCount].pwmChannel = -1;
    _pins[_pinCount].minValue = 0;
    _pins[_pinCount].maxValue = 1;

    _pinCount++;
    log("Added digital output: " + String(name) + " (GPIO " + String(pin) + ")");
}

void MvsConnect::addPWMOutput(int pin, const char* name, const char* description, int initialValue) {
    if (_pinCount >= MAX_PINS) {
        log("ERROR: Maximum pins reached!");
        return;
    }

    // Setup PWM (ESP32 Arduino Core 3.x API)
    ledcAttach(pin, 5000, 8);  // pin, frequency 5kHz, 8-bit resolution
    ledcWrite(pin, initialValue);

    _pins[_pinCount].pin = pin;
    _pins[_pinCount].name = String(name);
    _pins[_pinCount].description = String(description);
    _pins[_pinCount].isOutput = true;
    _pins[_pinCount].isPWM = true;
    _pins[_pinCount].value = initialValue;
    _pins[_pinCount].pwmChannel = pin;  // In new API, use pin directly
    _pins[_pinCount].minValue = 0;
    _pins[_pinCount].maxValue = 255;

    _pinCount++;
    log("Added PWM output: " + String(name) + " (GPIO " + String(pin) + ")");
}

void MvsConnect::addDigitalInput(int pin, const char* name, const char* description) {
    if (_pinCount >= MAX_PINS) {
        log("ERROR: Maximum pins reached!");
        return;
    }

    pinMode(pin, INPUT);

    _pins[_pinCount].pin = pin;
    _pins[_pinCount].name = String(name);
    _pins[_pinCount].description = String(description);
    _pins[_pinCount].isOutput = false;
    _pins[_pinCount].isPWM = false;
    _pins[_pinCount].value = digitalRead(pin);
    _pins[_pinCount].pwmChannel = -1;
    _pins[_pinCount].minValue = 0;
    _pins[_pinCount].maxValue = 1;

    _pinCount++;
    log("Added digital input: " + String(name) + " (GPIO " + String(pin) + ")");
}

// ============================================
// PIN CONTROL
// ============================================

void MvsConnect::setPinValue(int pin, int value) {
    MvsPin* p = findPin(pin);
    if (!p || !p->isOutput) return;

    // Constrain value
    value = constrain(value, p->minValue, p->maxValue);
    p->value = value;

    // Apply to hardware
    if (p->isPWM) {
        ledcWrite(pin, value);  // ESP32 Core 3.x uses pin directly
    } else {
        digitalWrite(pin, value);
    }

    log("Pin " + String(pin) + " set to " + String(value));
}

int MvsConnect::getPinValue(int pin) {
    MvsPin* p = findPin(pin);
    if (!p) return -1;

    if (!p->isOutput) {
        p->value = digitalRead(pin);
    }
    return p->value;
}

int MvsConnect::togglePin(int pin) {
    MvsPin* p = findPin(pin);
    if (!p || !p->isOutput || p->isPWM) return -1;

    int newValue = (p->value == LOW) ? HIGH : LOW;
    setPinValue(pin, newValue);
    return newValue;
}

MvsPin* MvsConnect::findPin(int pin) {
    for (int i = 0; i < _pinCount; i++) {
        if (_pins[i].pin == pin) {
            return &_pins[i];
        }
    }
    return nullptr;
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
    _preferences.begin(NVS_NAMESPACE, true);  // Read-only
    bool valid = _preferences.getBool(KEY_VALID, false);
    _preferences.end();
    return valid;
}

void MvsConnect::clearSavedWiFi() {
    _preferences.begin(NVS_NAMESPACE, false);  // Read-write
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
    _preferences.begin(NVS_NAMESPACE, false);  // Read-write
    _preferences.putString(KEY_SSID, ssid);
    _preferences.putString(KEY_PASSWORD, password);
    _preferences.putBool(KEY_VALID, true);
    _preferences.end();
    log("WiFi credentials saved to NVS for: " + ssid);
}

String MvsConnect::readSavedSSID() {
    _preferences.begin(NVS_NAMESPACE, true);  // Read-only
    String ssid = _preferences.getString(KEY_SSID, "");
    _preferences.end();
    return ssid;
}

String MvsConnect::readSavedPassword() {
    _preferences.begin(NVS_NAMESPACE, true);  // Read-only
    String password = _preferences.getString(KEY_PASSWORD, "");
    _preferences.end();
    return password;
}

void MvsConnect::updateWiFiConnectionStatus() {
    static bool wasConnecting = false;  // Track connection state change

    if (!_isConnecting && WiFi.status() != WL_CONNECTED) {
        if (_lastSSID.length() > 0) {
            _wifiStatus = "FAILED: Cannot connect to " + _lastSSID;
        } else {
            _wifiStatus = "READY: No WiFi configured";
        }
        wasConnecting = false;
        return;
    }

    if (_isConnecting) {
        wasConnecting = true;
        unsigned long elapsed = (millis() - _wifiConnectStart) / 1000;

        if (WiFi.status() == WL_CONNECTED) {
            _wifiStatus = "CONNECTED: " + _lastSSID + " IP:" + WiFi.localIP().toString();
            _isConnecting = false;
            log("Connected to " + _lastSSID + " (IP: " + WiFi.localIP().toString() + ")");

            // Restart mDNS to advertise on WiFi network (after WiFi transfer)
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

void MvsConnect::onPinChange(MvsConnectPinCallback callback) {
    _pinCallback = callback;
}

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
int MvsConnect::getPinCount() { return _pinCount; }
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
    // If authentication is disabled, allow all requests
    if (!_userAgentAuthEnabled) {
        return true;
    }

    // Check User-Agent header
    if (_server->hasHeader("User-Agent")) {
        String userAgent = _server->header("User-Agent");
        if (userAgent == _expectedUserAgent) {
            return true;
        }
        log("Unauthorized: Wrong User-Agent: " + userAgent);
    } else {
        log("Unauthorized: Missing User-Agent header");
    }

    // Send 401 Unauthorized response
    _server->send(401, "text/plain", "Unauthorized: Invalid User-Agent");
    return false;
}

// ============================================
// INTERNAL SETUP
// ============================================

void MvsConnect::setupMDNS() {
    // mDNS only works reliably on WiFi (STA) network, not on AP
    // On AP mode, clients already know the IP is 192.168.4.1
    if (WiFi.status() != WL_CONNECTED) {
        log("mDNS skipped: Not connected to WiFi network");
        return;
    }

    // Stop existing mDNS first (for re-initialization after WiFi connect)
    MDNS.end();

    // Create mDNS name - MUST contain "mvstech" for Android app discovery!
    // Android app filters: serviceName.toLowerCase().contains("mvstech")
    String mdnsName = _deviceName;
    mdnsName.replace(" ", "-");
    mdnsName.toLowerCase();

    // Ensure name contains "mvstech" for Android app discovery
    if (mdnsName.indexOf("mvstech") < 0) {
        mdnsName += "-mvstech";
    }

    if (MDNS.begin(mdnsName.c_str())) {
        // Register HTTP service for discovery
        // Service name format: "mdnsName._http._tcp.local"
        // Android app looks for services containing "mvstech" in the name
        MDNS.addService("http", "tcp", _port);
        MDNS.addServiceTxt("http", "tcp", "device", _deviceName.c_str());
        MDNS.addServiceTxt("http", "tcp", "version", _version.c_str());

        // Register mvstech service for specific discovery
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
    // This ensures addEndpoint() calls work reliably whether called before or after begin()
    _server->onNotFound([this]() {
        _server->send(404, "text/plain", "Not Found");
    });

    // Main page
    _server->on("/", HTTP_GET, [this]() { handleRoot(); });

    // Pin control API
    _server->on("/set", HTTP_GET, [this]() { handleSetPin(); });
    _server->on("/pins", HTTP_GET, [this]() { handleGetPins(); });
    _server->on("/info", HTTP_GET, [this]() { handleGetInfo(); });

    // WiFi transfer endpoints (internal - prefixed with __)
    // HTTP_ANY accepts both GET (Android app) and POST (future support)
    _server->on("/__mvsconnect/wifi/transfer", HTTP_ANY, [this]() { handleWiFiTransfer(); });
    _server->on("/__mvsconnect/wifi/status", HTTP_ANY, [this]() { handleWiFiStatus(); });
}

// ============================================
// ROUTE HANDLERS - PIN CONTROL
// ============================================

void MvsConnect::handleRoot() {
    if (!checkUserAgent()) return;
    _server->send(200, "text/html", generateHTML());
}

void MvsConnect::handleSetPin() {
    if (!checkUserAgent()) return;

    if (!_server->hasArg("pin") || !_server->hasArg("value")) {
        _server->send(400, "text/plain", "Missing pin or value");
        return;
    }

    int pin = _server->arg("pin").toInt();
    int value = _server->arg("value").toInt();

    MvsPin* p = findPin(pin);
    if (!p) {
        _server->send(404, "text/plain", "Pin not found");
        return;
    }

    if (!p->isOutput) {
        _server->send(400, "text/plain", "Pin is input only");
        return;
    }

    setPinValue(pin, value);

    // Call user callback
    if (_pinCallback) {
        _pinCallback(pin, value);
    }

    _server->send(200, "application/json", "{\"pin\":" + String(pin) + ",\"value\":" + String(p->value) + "}");
}

void MvsConnect::handleGetPins() {
    if (!checkUserAgent()) return;

    String json = "[";
    for (int i = 0; i < _pinCount; i++) {
        if (i > 0) json += ",";

        // Update input values
        if (!_pins[i].isOutput) {
            _pins[i].value = digitalRead(_pins[i].pin);
        }

        json += "{";
        json += "\"pin\":" + String(_pins[i].pin) + ",";
        json += "\"name\":\"" + _pins[i].name + "\",";
        json += "\"description\":\"" + _pins[i].description + "\",";
        json += "\"isOutput\":" + String(_pins[i].isOutput ? "true" : "false") + ",";
        json += "\"isPWM\":" + String(_pins[i].isPWM ? "true" : "false") + ",";
        json += "\"value\":" + String(_pins[i].value) + ",";
        json += "\"min\":" + String(_pins[i].minValue) + ",";
        json += "\"max\":" + String(_pins[i].maxValue);
        json += "}";
    }
    json += "]";

    _server->send(200, "application/json", json);
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
// HTML GENERATION
// ============================================

String MvsConnect::generateHTML() {
    // If custom HTML callback is set, return it as FULL page (no shell wrapper)
    // This gives complete page control for custom PWA/web apps
    if (_customHTMLCallback) {
        return _customHTMLCallback();
    }

    // Default pin control UI (only used when no custom HTML is set)
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>" + _deviceName + "</title>";
    html += "<style>" + generateCSS() + "</style>";
    html += "</head><body>";

    // Header
    html += "<div class='header'>";
    html += "<h1>" + _deviceName + "</h1>";
    html += "<span class='version'>v" + _version + "</span>";
    html += "</div>";

    // Status bar
    html += "<div class='status'>";
    html += "<span>AP: " + WiFi.softAPIP().toString() + "</span>";
    if (WiFi.status() == WL_CONNECTED) {
        html += "<span>WiFi: " + WiFi.localIP().toString() + "</span>";
    }
    html += "</div>";

    // Pin controls
    html += "<div class='container'>";
    html += generatePinCards();
    html += "</div>";

    // Footer
    html += "<div class='footer'>";
    html += "<p>Uptime: <span id='uptime'>0</span>s | Free Heap: <span id='heap'>0</span> KB</p>";
    html += "</div>";

    html += "<script>" + generateJS() + "</script>";
    html += "</body></html>";

    return html;
}

String MvsConnect::generateCSS() {
    return R"(
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
               background: #1a1a2e; color: #eee; min-height: 100vh; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                  padding: 20px; text-align: center; }
        .header h1 { font-size: 24px; margin-bottom: 5px; }
        .version { opacity: 0.8; font-size: 14px; }
        .status { background: #16213e; padding: 10px 20px; display: flex;
                  justify-content: space-around; font-size: 12px; }
        .container { padding: 15px; }
        .card { background: #16213e; border-radius: 12px; padding: 15px;
                margin-bottom: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .card-header { display: flex; justify-content: space-between; align-items: center;
                       margin-bottom: 10px; }
        .card-title { font-size: 16px; font-weight: 600; }
        .card-pin { font-size: 12px; opacity: 0.6; }
        .card-desc { font-size: 12px; opacity: 0.7; margin-bottom: 12px; }

        /* Toggle Switch */
        .toggle-container { display: flex; align-items: center; justify-content: space-between; }
        .toggle { position: relative; width: 60px; height: 32px; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
                  background: #394867; border-radius: 32px; transition: 0.3s; }
        .slider:before { position: absolute; content: ''; height: 24px; width: 24px;
                         left: 4px; bottom: 4px; background: white; border-radius: 50%;
                         transition: 0.3s; }
        input:checked + .slider { background: #667eea; }
        input:checked + .slider:before { transform: translateX(28px); }
        .toggle-label { font-size: 14px; font-weight: 500; }

        /* PWM Slider */
        .pwm-container { }
        .pwm-value { text-align: center; font-size: 24px; font-weight: bold;
                     color: #667eea; margin-bottom: 10px; }
        .pwm-slider { width: 100%; height: 8px; border-radius: 4px; background: #394867;
                      outline: none; -webkit-appearance: none; }
        .pwm-slider::-webkit-slider-thumb { -webkit-appearance: none; width: 24px;
                      height: 24px; border-radius: 50%; background: #667eea; cursor: pointer; }

        /* Input display */
        .input-value { font-size: 20px; text-align: center; padding: 15px;
                       background: #0f3460; border-radius: 8px; }
        .input-value.high { color: #4ade80; }
        .input-value.low { color: #f87171; }

        .footer { text-align: center; padding: 20px; font-size: 12px; opacity: 0.6; }
        .custom { margin-top: 10px; }
    )";
}

String MvsConnect::generateJS() {
    return R"(
        function setPin(pin, value) {
            fetch('/set?pin=' + pin + '&value=' + value)
                .then(r => r.json())
                .then(d => console.log('Pin', d.pin, '=', d.value))
                .catch(e => console.error(e));
        }

        function togglePin(pin, checkbox) {
            setPin(pin, checkbox.checked ? 1 : 0);
        }

        function setPWM(pin, slider) {
            document.getElementById('pwm-val-' + pin).textContent = slider.value;
            setPin(pin, slider.value);
        }

        function updateStatus() {
            fetch('/info')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('uptime').textContent = d.uptime;
                    document.getElementById('heap').textContent = Math.round(d.heap / 1024);
                })
                .catch(e => {});

            fetch('/pins')
                .then(r => r.json())
                .then(pins => {
                    pins.forEach(p => {
                        if (!p.isOutput) {
                            var el = document.getElementById('input-' + p.pin);
                            if (el) {
                                el.textContent = p.value ? 'HIGH' : 'LOW';
                                el.className = 'input-value ' + (p.value ? 'high' : 'low');
                            }
                        }
                    });
                })
                .catch(e => {});
        }

        setInterval(updateStatus, 2000);
        updateStatus();
    )";
}

String MvsConnect::generatePinCards() {
    String html = "";

    for (int i = 0; i < _pinCount; i++) {
        MvsPin& p = _pins[i];

        html += "<div class='card'>";
        html += "<div class='card-header'>";
        html += "<span class='card-title'>" + p.name + "</span>";
        html += "<span class='card-pin'>GPIO " + String(p.pin) + "</span>";
        html += "</div>";

        if (p.description.length() > 0) {
            html += "<div class='card-desc'>" + p.description + "</div>";
        }

        if (p.isOutput) {
            if (p.isPWM) {
                // PWM slider
                html += "<div class='pwm-container'>";
                html += "<div class='pwm-value' id='pwm-val-" + String(p.pin) + "'>" + String(p.value) + "</div>";
                html += "<input type='range' class='pwm-slider' min='0' max='255' value='" + String(p.value) + "' ";
                html += "oninput='setPWM(" + String(p.pin) + ", this)'>";
                html += "</div>";
            } else {
                // Toggle switch
                html += "<div class='toggle-container'>";
                html += "<span class='toggle-label'>" + String(p.value ? "ON" : "OFF") + "</span>";
                html += "<label class='toggle'>";
                html += "<input type='checkbox' " + String(p.value ? "checked" : "") + " ";
                html += "onchange='togglePin(" + String(p.pin) + ", this)'>";
                html += "<span class='slider'></span>";
                html += "</label>";
                html += "</div>";
            }
        } else {
            // Input display
            html += "<div class='input-value " + String(p.value ? "high" : "low") + "' id='input-" + String(p.pin) + "'>";
            html += String(p.value ? "HIGH" : "LOW");
            html += "</div>";
        }

        html += "</div>";
    }

    if (_pinCount == 0) {
        html += "<div class='card'>";
        html += "<p style='text-align:center;opacity:0.6'>No pins configured.<br>Use addDigitalOutput() or addPWMOutput() in setup().</p>";
        html += "</div>";
    }

    return html;
}

// ============================================
// HELPERS
// ============================================

void MvsConnect::log(const String& message) {
    if (_debug) {
        Serial.println("[MvsConnect] " + message);
    }
}
