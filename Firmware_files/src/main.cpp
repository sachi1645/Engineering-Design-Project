#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <WiFiUdp.h>

void handleRoot();
void handleSave();
void startCaptivePortal();
void connectToWiFi();
void sendAlert(bool state);
void toggleAlertState();
void checkForResetCondition();
void triggerReset();
bool reconnectWiFi();
String discoverServer();
void blinkLED(bool stop = false, unsigned long interval = 0);
void printNetworkInfo();
String improvedDiscoverServer();
void improvedCaptivePortal();
bool enhancedWiFiConnect();
void networkDiagnostics();

// Pins
const int buttonPin = 25;
const int ledPin = 18;
const int bootButtonPin = 0;

// Network
const int UDP_PORT = 12345;
const char* UDP_REQUEST = "WHERE_IS_SERVER";
WiFiUDP udp;

// Configuration
struct Config {
  char ssid[32];
  char password[32];
  char deviceName[32];
  bool configured;
};

// State
Config config;
WebServer server(80);
DNSServer dnsServer;
bool alertState = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 500;
unsigned long buttonPressStartTime = 0;
bool resetTriggered = false;
bool serverConnected = false;

// LED blinking variables
bool isBlinking = false;
unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 300; // Fast blink interval in ms
const unsigned long serverBlinkInterval = 1000; // Slow blink interval for server disconnect
bool ledState = false;
unsigned long lastServerCheckTime = 0;
const unsigned long serverCheckInterval = 10000; // Check server every 10 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32 Emergency Alert System Starting ===");
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(bootButtonPin, INPUT);

  // Initialize LED state
  digitalWrite(ledPin, LOW);
  ledState = false;
  alertState = false;
  serverConnected = false;

  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, config);

  Serial.println("Checking for reset condition...");
  checkForResetCondition();

  if (!config.configured) {
    Serial.println("Device not configured, starting setup mode...");
    // Start blinking LED to indicate setup mode
    isBlinking = true;
    startCaptivePortal();
  } else {
    Serial.println("Device configured, connecting to WiFi...");
    // Start blinking LED while connecting to WiFi
    isBlinking = true;
    connectToWiFi();
  }
  
  networkDiagnostics();
}

void loop() {
  // Handle LED blinking if necessary
  if (isBlinking) {
    unsigned long currentInterval = !config.configured ? blinkInterval : 
                                   (WiFi.status() == WL_CONNECTED && !serverConnected) ? serverBlinkInterval : 
                                   blinkInterval;
    blinkLED(false, currentInterval);
  }
  
  // Reset button check
  if (digitalRead(bootButtonPin) == LOW && !resetTriggered) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
      Serial.println("Boot button pressed, hold for factory reset...");
    } else if (millis() - buttonPressStartTime > 3000) {
      triggerReset();
    }
  } else {
    buttonPressStartTime = 0;
  }

  // If configured, check connection status periodically
  if (config.configured && WiFi.status() == WL_CONNECTED) {
    // Periodically check if server is available
    if (millis() - lastServerCheckTime > serverCheckInterval) {
      lastServerCheckTime = millis();
      
      Serial.println("Performing periodic server check...");
      String serverIP = discoverServer();
      
      if (serverIP.isEmpty()) {
        Serial.println("Server not found on this check");
        if (serverConnected || !isBlinking) {
          // Server disconnected, start slow blinking
          serverConnected = false;
          isBlinking = true;
          Serial.println("Server disconnected/unavailable - starting indicator blinking");
        }
      } else {
        Serial.print("Server found at: "); Serial.println(serverIP);
        if (!serverConnected) {
          // Server connected, stop blinking
          serverConnected = true;
          
          if (isBlinking) {
            // Stop blinking and set LED to reflect alert state
            blinkLED(true);
            Serial.println("Server connected - stopping indicator blinking");
          }
        }
      }
    }
    
    // Alert button check with debounce (only if server is connected)
    if (serverConnected && digitalRead(buttonPin) == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      Serial.println("Alert button pressed");
      toggleAlertState();
    }
  } else if (config.configured && WiFi.status() != WL_CONNECTED) {
    // If WiFi disconnected, attempt to reconnect periodically
    if (millis() - lastServerCheckTime > serverCheckInterval) {
      lastServerCheckTime = millis();
      Serial.println("WiFi disconnected, attempting to reconnect...");
      reconnectWiFi();
    }
  }
  
  // Handle captive portal if not configured
  if (!config.configured) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

void blinkLED(bool stop, unsigned long interval) {
  if (stop) {
    isBlinking = false;
    // Ensure LED properly reflects current alert state when blinking stops
    ledState = alertState;
    digitalWrite(ledPin, alertState ? HIGH : LOW);
    Serial.print("Blinking stopped. LED set to: "); Serial.println(alertState ? "ON" : "OFF");
    return;
  }
  
  unsigned long actualInterval = interval > 0 ? interval : blinkInterval;
  
  if (isBlinking && millis() - lastBlinkTime > actualInterval) {
    lastBlinkTime = millis();
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? HIGH : LOW);
  }
}

void toggleAlertState() {
  alertState = !alertState;
  ledState = alertState; // Keep ledState in sync with alertState
  digitalWrite(ledPin, alertState ? HIGH : LOW);
  Serial.print("Alert state toggled to: "); Serial.println(alertState ? "ON" : "OFF");
  sendAlert(alertState);
}

void sendAlert(bool state) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send alert");
    if (!reconnectWiFi()) {
      // Could not reconnect, revert alert state
      alertState = !alertState;
      ledState = alertState;
      digitalWrite(ledPin, alertState ? HIGH : LOW);
      Serial.println("Could not reconnect to WiFi, alert state reverted");
      return;
    }
  }

  String serverIP = discoverServer();
  if (serverIP.isEmpty()) {
    Serial.println("Server discovery failed, cannot send alert");
    // Server not available, set status
    serverConnected = false;
    isBlinking = true;
    // Revert alert state
    alertState = !alertState;
    Serial.println("Server not available, alert state reverted");
    return;
  }

  // Server is available
  serverConnected = true;
  
  HTTPClient http;
  String url = "http://" + serverIP + ":5000/alert";
  Serial.print("Sending alert to: "); Serial.println(url);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "name=" + String(config.deviceName);
  Serial.print("POST data: "); Serial.println(postData);
  
  int httpCode = http.POST(postData);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("Alert "); 
    Serial.print(state ? "activated" : "deactivated");
    Serial.print(" with HTTP code: ");
    Serial.print(httpCode);
    Serial.print(" - Response: ");
    Serial.println(response);
  } else {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(httpCode).c_str());
    // Revert state if failed
    alertState = !alertState;
    ledState = alertState; // Keep ledState in sync with alertState
    digitalWrite(ledPin, alertState ? HIGH : LOW);
    Serial.println("HTTP request failed, alert state reverted");
  }
  http.end();
}

bool reconnectWiFi() {
  // Start blinking while reconnecting
  isBlinking = true;
  Serial.print("Reconnecting to WiFi: "); Serial.println(config.ssid);
  
  WiFi.reconnect();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nReconnected to WiFi!");
    printNetworkInfo();
    
    // Check server connection
    String serverIP = discoverServer();
    if (serverIP.isEmpty()) {
      Serial.println("Server unavailable after WiFi reconnect.");
      serverConnected = false;
      // Continue blinking but at slower rate
      isBlinking = true;
      return true; // WiFi connected but server not available
    } else {
      Serial.println("Server found at: " + serverIP);
      serverConnected = true;
      // Stop blinking, everything is connected
      blinkLED(true);
      return true;
    }
  } else {
    Serial.println("\nWiFi reconnection failed!");
    serverConnected = false;
    // Continue fast blinking to indicate WiFi connection issue
    return false;
  }
}

String discoverServer() {
  return improvedDiscoverServer();
}

String improvedDiscoverServer() {
  Serial.println("Attempting server discovery...");
  
  // Try 3 times
  for (int attempt = 0; attempt < 3; attempt++) {
    Serial.print("Discovery attempt "); Serial.println(attempt + 1);
    
    udp.beginPacket("255.255.255.255", UDP_PORT);
    udp.write((const uint8_t*)UDP_REQUEST, strlen(UDP_REQUEST));
    udp.endPacket();
    
    unsigned long start = millis();
    while (millis() - start < 1000) { // 1 second per attempt
      int packetSize = udp.parsePacket();
      if (packetSize) {
        char buffer[16];
        int len = udp.read(buffer, 15);
        buffer[len] = '\0';
        Serial.print("Server found at: "); Serial.println(buffer);
        return String(buffer);
      }
      delay(50);
    }
    Serial.println("No response in this attempt");
  }
  
  Serial.println("Server discovery failed after 3 attempts");
  return "";
}

void checkForResetCondition() {
  if (digitalRead(bootButtonPin) == LOW) {
    delay(100); // Debounce
    unsigned long startTime = millis();
    while (digitalRead(bootButtonPin) == LOW) {
      if (millis() - startTime > 3000) {
        triggerReset();
        return;
      }
    }
  }
}

void triggerReset() {
  Serial.println("Factory reset triggered!");
  digitalWrite(ledPin, HIGH);
  
  Config blank = {0};
  EEPROM.put(0, blank);
  EEPROM.commit();
  
  delay(1000);
  digitalWrite(ledPin, LOW);
  resetTriggered = true;
  ESP.restart();
}

void startCaptivePortal() {
  improvedCaptivePortal();
}

void improvedCaptivePortal() {
  Serial.println("Starting improved captive portal...");
  
  // Use a more recognizable AP name
  WiFi.softAP("EMERGENCY ALERT SETUP");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  // Standard DNS port
  dnsServer.start(53, "*", myIP);
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/generate_204", handleRoot);  // Android captive portal detection
  server.on("/favicon.ico", handleRoot);   // Browser icon request
  
  // Handle common captive portal detection URLs
  server.on("/hotspot-detect.html", handleRoot); // iOS/macOS
  server.on("/ncsi.txt", handleRoot);            // Windows
  server.on("/connecttest.txt", handleRoot);     // Windows
  
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = R"=====(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Device Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial, sans-serif; max-width: 400px; margin: 0 auto; padding: 20px; }
      h1 { color: #444; text-align: center; }
      form { background: #f9f9f9; padding: 20px; border-radius: 5px; }
      input { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; }
      input[type=submit] { background: #4CAF50; color: white; border: none; }
    </style>
  </head>
  <body>
    <h1>Emergency Alert Setup</h1>
    <form action="/save" method="post">
      <label for="ssid">WiFi Network:</label>
      <input type="text" id="ssid" name="ssid" required placeholder="Your WiFi name">
      <label for="password">WiFi Password:</label>
      <input type="password" id="password" name="password" placeholder="Your WiFi password">
      <label for="deviceName">Device Name:</label>
      <input type="text" id="deviceName" name="deviceName" required placeholder="e.g., John's Device">
      <input type="submit" value="Save Configuration">
    </form>
  </body>
  </html>
  )=====";
  server.send(200, "text/html", html);
}

void handleSave() {
  strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid));
  strncpy(config.password, server.arg("password").c_str(), sizeof(config.password));
  strncpy(config.deviceName, server.arg("deviceName").c_str(), sizeof(config.deviceName));
  config.configured = true;
  
  EEPROM.put(0, config);
  EEPROM.commit();
  
  server.send(200, "text/html", 
    "<h1>Configuration Saved!</h1><p>Device will restart and connect to your network.</p>");
  delay(2000);
  ESP.restart();
}

void connectToWiFi() {
  enhancedWiFiConnect(); // Call the function but don't return its value
}

bool enhancedWiFiConnect() {
  Serial.println("Connecting to WiFi: " + String(config.ssid));
  
  WiFi.disconnect();
  delay(100);
  WiFi.begin(config.ssid, config.password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("");
  
  if (WiFi.status() == WL_CONNECTED) {
    printNetworkInfo();
    
    // Now check for server availability
    String serverIP = discoverServer();
    if (serverIP.isEmpty()) {
      Serial.println("Server unavailable. LED will indicate disconnected state.");
      serverConnected = false;
      // Continue blinking but at slower rate to indicate server disconnect
      isBlinking = true;
      return true;
    } else {
      Serial.println("Server found at: " + serverIP);
      serverConnected = true;
      // Stop blinking, server is available
      blinkLED(true);
      return true;
    }
  } else {
    Serial.println("WiFi connection failed! Starting config portal...");
    config.configured = false;
    EEPROM.put(0, config);
    EEPROM.commit();
    serverConnected = false;
    startCaptivePortal();
    return false;
  }
}

void printNetworkInfo() {
  Serial.println("\n--- Network Diagnostics ---");
  Serial.print("WiFi Status: ");
  switch (WiFi.status()) {
    case WL_CONNECTED: Serial.println("Connected"); break;
    case WL_NO_SHIELD: Serial.println("No WiFi shield"); break;
    case WL_IDLE_STATUS: Serial.println("Idle"); break;
    case WL_NO_SSID_AVAIL: Serial.println("No SSID available"); break;
    case WL_SCAN_COMPLETED: Serial.println("Scan completed"); break;
    case WL_CONNECT_FAILED: Serial.println("Connection failed"); break;
    case WL_CONNECTION_LOST: Serial.println("Connection lost"); break;
    case WL_DISCONNECTED: Serial.println("Disconnected"); break;
    default: Serial.println("Unknown"); break;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    Serial.print("Subnet Mask: "); Serial.println(WiFi.subnetMask());
    Serial.print("Gateway IP: "); Serial.println(WiFi.gatewayIP());
    Serial.print("DNS Server: "); Serial.println(WiFi.dnsIP());
    Serial.print("Signal Strength (RSSI): "); Serial.println(WiFi.RSSI());
  }
  
  Serial.print("Server Connected: "); Serial.println(serverConnected ? "Yes" : "No");
  Serial.println("----------------------------");
}

void networkDiagnostics() {
  Serial.println("\n====== EMERGENCY ALERT SYSTEM DIAGNOSTICS ======");
  Serial.print("Device Name: "); 
  if (config.configured) {
    Serial.println(config.deviceName);
  } else {
    Serial.println("Not configured");
  }
  Serial.print("Configured: "); Serial.println(config.configured ? "Yes" : "No");
  
  if (config.configured) {
    Serial.print("WiFi SSID: "); Serial.println(config.ssid);
    // Don't print the password for security
  }
  
  printNetworkInfo();
  Serial.println("===============================================");
}