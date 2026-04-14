#include <Arduino.h>

#ifdef BOARD_ESP32C3
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
  #include <Preferences.h>
  #define LED_PIN 8
#endif

#ifdef BOARD_RP2040
  #include <Adafruit_NeoPixel.h>
  #define NEOPIXEL_PIN 16
  #define NEOPIXEL_COUNT 1
  Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
  uint32_t ledColor = 0;
#endif

enum State { IDLE, RESPONSE, WORKING, DONE };
State currentState = IDLE;
unsigned long stateStart = 0;
unsigned long lastToggle = 0;
bool ledOn = false;
String serialBuffer = "";

#ifdef BOARD_ESP32C3
  bool wifiMode = false;
  bool apMode = false;
  WebServer server(80);
  Preferences prefs;
#endif

void setLed(bool on) {
  #ifdef BOARD_ESP32C3
    digitalWrite(LED_PIN, on ? LOW : HIGH);
  #endif
  #ifdef BOARD_RP2040
    pixel.setPixelColor(0, on ? ledColor : 0);
    pixel.show();
  #endif
  ledOn = on;
}

void setState(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd == "RESPONSE") {
    currentState = RESPONSE;
    stateStart = millis();
    #ifdef BOARD_RP2040
      ledColor = pixel.Color(80, 0, 0); // red
    #endif
  } else if (cmd == "WORKING") {
    currentState = WORKING;
    stateStart = millis();
    #ifdef BOARD_RP2040
      ledColor = pixel.Color(80, 40, 0); // orange
    #endif
  } else if (cmd == "DONE") {
    currentState = DONE;
    stateStart = millis();
    #ifdef BOARD_RP2040
      ledColor = pixel.Color(0, 80, 0); // green
    #endif
  } else if (cmd == "OFF") {
    currentState = IDLE;
    setLed(false);
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        setState(serialBuffer);
      }
      serialBuffer = "";
    } else if (c >= 'A' && c <= 'z') {
      serialBuffer += c;
    }
  }
}

// ---- WiFi (ESP32-C3 only) ----
#ifdef BOARD_ESP32C3

void handleCommand(String cmd) {
  setState(cmd);
  server.send(200, "text/plain", "OK:" + cmd);
  Serial.println("WiFi cmd: " + cmd);
}

const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px}
input{width:100%;padding:8px;margin:4px 0 12px;box-sizing:border-box}
button{background:#007bff;color:#fff;border:none;padding:10px 20px;width:100%;cursor:pointer}
</style></head><body>
<h2>ClauBlink WiFi Setup</h2>
<form action="/save" method="POST">
<label>SSID</label><input name="ssid" required>
<label>Password</label><input name="pass" type="password">
<button type="submit">Connect</button>
</form></body></html>
)rawliteral";

void handleConfigPage() {
  server.send(200, "text/html", CONFIG_PAGE);
}

void handleSaveWiFi() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  server.send(200, "text/html", "<h2>Saved! Restarting...</h2>");
  Serial.println("WiFi credentials saved. Restarting...");
  delay(1000);
  ESP.restart();
}

void handleResetWiFi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  server.send(200, "text/plain", "WiFi credentials cleared. Restarting...");
  Serial.println("WiFi reset. Restarting...");
  delay(1000);
  ESP.restart();
}

void startHTTPServer() {
  server.on("/RESPONSE", []() { handleCommand("RESPONSE"); });
  server.on("/WORKING", []() { handleCommand("WORKING"); });
  server.on("/DONE", []() { handleCommand("DONE"); });
  server.on("/OFF", []() { handleCommand("OFF"); });
  server.on("/reset", handleResetWiFi);
  server.begin();
  Serial.println("HTTP server started on port 80");
}

bool connectToSavedWiFi() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  Serial.print("Saved SSID: '");
  Serial.print(ssid);
  Serial.println("'");

  if (ssid.isEmpty()) {
    Serial.println("No saved WiFi credentials.");
    return false;
  }

  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Connection failed.");
  return false;
}

void startAPPortal() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ClauBlink", "12345678");

  Serial.print("AP portal started. SSID: ClauBlink, IP: ");
  Serial.println(WiFi.softAPIP());

  for (int i = 0; i < 6; i++) {
    setLed(i % 2 == 0);
    delay(200);
  }
  setLed(false);

  server.on("/", handleConfigPage);
  server.on("/save", HTTP_POST, handleSaveWiFi);
  server.begin();
}

void setupWiFi() {
  if (connectToSavedWiFi()) {
    wifiMode = true;

    if (MDNS.begin("claublink")) {
      Serial.println("mDNS: claublink.local");
    }

    server.on("/", []() {
      server.send(200, "text/plain", "CLAUBLINK OK. States: /RESPONSE /WORKING /DONE /OFF /reset");
    });
    startHTTPServer();
  } else {
    startAPPortal();
  }
}

#endif // BOARD_ESP32C3

void setup() {
  Serial.begin(115200);

  #ifdef BOARD_ESP32C3
    pinMode(LED_PIN, OUTPUT);
  #endif
  #ifdef BOARD_RP2040
    pixel.begin();
    pixel.setBrightness(80);
  #endif

  setLed(false);
  delay(2000);

  Serial.println("=== CLAUBLINK BOOT ===");

  #ifdef BOARD_ESP32C3
    setupWiFi();
  #endif

  Serial.println("CLAUBLINK READY");
}

void loop() {
  handleSerial();

  #ifdef BOARD_ESP32C3
    if (wifiMode || apMode) {
      server.handleClient();
    }
  #endif

  unsigned long now = millis();
  unsigned long elapsed = now - stateStart;

  switch (currentState) {
    case IDLE:
      break;

    case RESPONSE:
      #ifdef BOARD_RP2040
        ledColor = pixel.Color(255, 0, 255);
      #endif
      setLed(true);
      break;

    case WORKING:
      #ifdef BOARD_RP2040
        ledColor = pixel.Color(0, 0, 255);
      #endif
      if (now - lastToggle >= 200) {
        setLed(!ledOn);
        lastToggle = now;
      }
      break;

    case DONE:
      #ifdef BOARD_RP2040
        ledColor = pixel.Color(0, 80, 0);
      #endif
      setLed(true);
      break;
  }
}
