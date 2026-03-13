# MvsConnect - ESP32 IoT Web Server Library

Simple library to create web-based device control interfaces for ESP32. Register your pins and get an automatic mobile-friendly UI with built-in WiFi credential transfer.

## Features

- **Easy Pin Registration** - Just call `addDigitalOutput()` or `addPWMOutput()`
- **Mobile-Friendly UI** - Responsive web interface with toggle switches and sliders
- **Built-in WiFi Transfer** - Receive WiFi credentials from Android app (no extra library needed!)
- **mDNS Discovery** - Android app can find your device automatically
- **Custom HTML** - Add your own content to the web page
- **Custom Endpoints** - Create your own REST API
- **Callback System** - React to pin changes and WiFi credential events

## Installation

1. Copy the `MvsConnect` folder to your Arduino libraries folder:
   - Windows: `Documents\Arduino\libraries\`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`

2. Optional: Install companion library for OTA updates:
   - `MvsOTA_ESP32` - Over-the-air firmware updates

3. Restart Arduino IDE

## Quick Start

```cpp
#include <WiFi.h>
#include <mvsconnect.h>
#include <mvsota_esp32.h>  // Optional: for OTA updates

#define DEVICE_NAME "MyDevice"
#define VERSION "1.0.0"

MvsConnect device(DEVICE_NAME, VERSION);
MvsOTA mvsota;  // Optional

void setup() {
    Serial.begin(115200);

    // Start WiFi Access Point
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("MyDevice_mvstech", "mvstech9867");

    // Register pins
    device.addDigitalOutput(2, "LED", "Built-in LED", LOW);
    device.addPWMOutput(5, "Brightness", "LED brightness", 0);

    // Start MvsConnect (handles web UI AND WiFi transfer!)
    device.begin();

    // Try to connect to saved WiFi
    if (device.connectToSavedWiFi(10000)) {
        Serial.println("Connected to saved WiFi!");
    }

    // Optional: Start OTA service
    mvsota.begin(DEVICE_NAME, VERSION);
}

void loop() {
    device.handle();
    mvsota.handle();  // Optional
}
```

## API Reference

### Constructor

```cpp
MvsConnect device(deviceName, version, port);
```
- `deviceName` - Device name (shown in UI)
- `version` - Firmware version string
- `port` - Web server port (default: 7689, matches Android app)

### Pin Registration

```cpp
// Digital output (ON/OFF toggle)
device.addDigitalOutput(pin, name, description, initialValue);

// PWM output (0-255 slider)
device.addPWMOutput(pin, name, description, initialValue);

// Digital input (read-only)
device.addDigitalInput(pin, name, description);
```

### Pin Control

```cpp
device.setPinValue(pin, value);  // Set pin value
int val = device.getPinValue(pin);  // Get pin value
device.togglePin(pin);  // Toggle digital output
```

### WiFi Credential Methods

```cpp
// Connect to saved WiFi credentials
if (device.connectToSavedWiFi(10000)) {
    Serial.println("Connected!");
}

// Check if credentials are saved
if (device.hasSavedWiFi()) { ... }

// Clear saved credentials
device.clearSavedWiFi();

// Check connection status
if (device.isWiFiConnected()) { ... }
String status = device.getWiFiStatus();
```

### Callbacks

```cpp
// Called when pin changes from web UI
device.onPinChange([](int pin, int value) {
    Serial.printf("Pin %d = %d\n", pin, value);
});

// Called when WiFi credentials received from Android app
device.onWiFiCredentialsReceived([](const String& ssid) {
    Serial.println("Received credentials for: " + ssid);
});

// Add custom HTML to page
device.setCustomHTML([]() {
    return "<div>Temperature: 25.5 C</div>";
});
```

### Custom Endpoints

```cpp
device.addEndpoint("/api/data", []() {
    device.getServer()->send(200, "text/plain", "Hello!");
});
```

## Port Summary

| Library | Port | Configurable | Purpose |
|---------|------|--------------|---------|
| MvsConnect | 7689 (default) | Yes | Web UI + WiFi credential transfer |
| MvsOTA_ESP32 | 8089 (default) | Yes | Firmware updates |

## WiFi Transfer Endpoints (Internal)

MvsConnect automatically handles these endpoints for the Android app:
- `/__mvsconnect/wifi/transfer` - Receive WiFi credentials
- `/__mvsconnect/wifi/status` - Check connection status

## Android App Integration

The mvsConnect Android app can:
1. **Discover devices** - Finds ESP32 via WiFi scan (SSID must end with `_mvstech`)
2. **Control pins** - Uses the web UI at configured port (default: 7689)
3. **Transfer WiFi** - Sends credentials via MvsConnect (same port)
4. **Update firmware** - OTA updates via MvsOTA_ESP32 (port 8089)

## Examples

See the `examples` folder:
- **BasicLED** - Simple LED control with WiFi transfer and OTA
- **AdvancedPinControl** - Multiple pins, custom HTML, custom APIs

## License

MIT License - Feel free to use in your projects!

## Author

MV Solutions
