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
  #define NUM_LEDS 5
  Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif

#define MAX_SLOTS 5

enum State { IDLE, RESPONSE, WORKING, DONE };
State slotStates[MAX_SLOTS] = { IDLE, IDLE, IDLE, IDLE, IDLE };
unsigned long slotToggle[MAX_SLOTS] = { 0 };
unsigned long slotStart[MAX_SLOTS] = { 0 };
bool slotLedOn[MAX_SLOTS] = { false };

String serialBuffer = "";

#ifdef BOARD_ESP32C3
  bool wifiMode = false;
  bool apMode = false;
  WebServer server(80);
  Preferences prefs;
#endif

#ifdef BOARD_RP2040
uint32_t getColor(State state) {
  switch (state) {
    case RESPONSE: return strip.Color(255, 0, 0);   // red
    case WORKING:  return strip.Color(0, 0, 255);   // blue
    case DONE:     return strip.Color(0, 80, 0);     // green
    default:       return 0;
  }
}
#endif

void updateLeds() {
  #ifdef BOARD_RP2040
    strip.show();
  #endif
}

void setSlotLed(int slot, bool on) {
  if (slot < 0 || slot >= MAX_SLOTS) return;
  #ifdef BOARD_ESP32C3
    if (slot == 0) {
      digitalWrite(LED_PIN, on ? LOW : HIGH);
    }
  #endif
  #ifdef BOARD_RP2040
    strip.setPixelColor(slot, on ? getColor(slotStates[slot]) : 0);
  #endif
  slotLedOn[slot] = on;
}

void setState(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  // Parse STATE:SLOT format
  String state = cmd;
  int slot = 0;
  int colonIdx = cmd.indexOf(':');
  if (colonIdx > 0) {
    state = cmd.substring(0, colonIdx);
    slot = cmd.substring(colonIdx + 1).toInt();
  }
  if (slot < 0 || slot >= MAX_SLOTS) slot = 0;

  if (state == "RESPONSE") {
    slotStates[slot] = RESPONSE;
    slotStart[slot] = millis();
  } else if (state == "WORKING") {
    slotStates[slot] = WORKING;
    slotStart[slot] = millis();
  } else if (state == "DONE") {
    slotStates[slot] = DONE;
    slotStart[slot] = millis();
  } else if (state == "OFF") {
    slotStates[slot] = IDLE;
    setSlotLed(slot, false);
    updateLeds();
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
    } else if ((c >= 'A' && c <= 'z') || c == ':' || (c >= '0' && c <= '9')) {
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
  server.on("/RESPONSE", []() { handleCommand("RESPONSE?slot=" + server.arg("slot")); });
  server.on("/WORKING", []() { handleCommand("WORKING?slot=" + server.arg("slot")); });
  server.on("/DONE", []() { handleCommand("DONE?slot=" + server.arg("slot")); });
  server.on("/OFF", []() { handleCommand("OFF?slot=" + server.arg("slot")); });
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
    strip.begin();
    strip.setBrightness(80);
    strip.clear();
    strip.show();
  #endif

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

  for (int i = 0; i < MAX_SLOTS; i++) {
    switch (slotStates[i]) {
      case IDLE:
        break;

      case RESPONSE:
        if (now - slotToggle[i] >= 200) {
          setSlotLed(i, !slotLedOn[i]);
          slotToggle[i] = now;
        }
        break;

      case WORKING:
        if (now - slotToggle[i] >= 800) {
          setSlotLed(i, !slotLedOn[i]);
          slotToggle[i] = now;
        }
        break;

      case DONE:
        setSlotLed(i, true);
        break;
    }
  }

  updateLeds();
}
