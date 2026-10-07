/*
 * AquaSense: continuous pH, ORP and temperature on an ESP32.
 *
 *   pH probe  -> isolated EZO-pH board  --\
 *   ORP probe -> isolated EZO-ORP board ---+-- I2C (21/22) --> ESP32 --> OLED
 *   DS18B20 temperature probe -- GPIO 4 --/                      |
 *                                                     Wi-Fi --> your server
 *
 * Every 2 s: read temperature, send it to the pH board for temperature
 * compensation, read pH and ORP, drop bad readings, average the last 10.
 * Every 60 s (configurable): POST the averages as JSON to your server.
 *
 * Open the serial monitor at 115200 baud (newline line ending) and type
 * "help" for the setup and calibration commands.
 *
 * Libraries: OneWire, DallasTemperature, U8g2. Board: ESP32 Dev Module.
 */

#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

#include "config.h"
#include "ezo.h"
#include "filters.h"
#include "settings.h"

#if defined(AQUASENSE_OLED_SH1106)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#elif defined(AQUASENSE_OLED_SSD1309)
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
#endif

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature tempProbe(&oneWire);
Ezo phBoard(EZO_PH_ADDR, "pH");
Ezo orpBoard(EZO_ORP_ADDR, "ORP");

// Possible range and largest believable change between two readings 2 s apart.
SmoothedReading temperature({-10.0f, 80.0f, 2.0f});  // deg C
SmoothedReading ph({0.0f, 14.0f, 0.5f});
SmoothedReading orp({-1020.0f, 1020.0f, 100.0f});  // mV

Settings settings;

uint32_t lastReadMs = 0;
uint32_t lastPostMs = 0;
uint32_t lastWifiAttemptMs = 0;
int lastPostStatus = 0;  // HTTP status of the last upload, or a negative error
bool printReadings = true;

// ---------------------------------------------------------------- readings --

void readSensors() {
  tempProbe.requestTemperatures();  // ~750 ms at 12-bit resolution
  float t = tempProbe.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) {
    temperature.miss();
  } else {
    temperature.add(t);
  }

  // Temperature compensation: the pH board forgets this at power-off,
  // so send it before every reading.
  if (temperature.has_value()) {
    char cmd[16];
    char reply[16];
    snprintf(cmd, sizeof(cmd), "T,%.1f", temperature.value());
    phBoard.query(cmd, reply, sizeof(reply));
  }

  // Start both readings, then wait once for both.
  bool phSent = phBoard.send("R");
  bool orpSent = orpBoard.send("R");
  delay(900);

  char reply[40];
  if (phSent && phBoard.read(reply, sizeof(reply)) == EZO_OK && reply[0]) {
    ph.add(atof(reply));
  } else {
    ph.miss();
  }
  if (orpSent && orpBoard.read(reply, sizeof(reply)) == EZO_OK && reply[0]) {
    orp.add(atof(reply));
  } else {
    orp.miss();
  }
}

void formatValue(char *out, size_t size, const SmoothedReading &r, int decimals) {
  if (r.has_value()) {
    snprintf(out, size, "%.*f", decimals, r.value());
  } else {
    snprintf(out, size, "--");
  }
}

const char *wifiText() {
  if (!settings.ssid[0]) return "not set up";
  if (WiFi.status() != WL_CONNECTED) return "connecting";
  return "connected";
}

void printToSerial() {
  char t[12], p[12], o[12];
  formatValue(t, sizeof(t), temperature, 2);
  formatValue(p, sizeof(p), ph, 2);
  formatValue(o, sizeof(o), orp, 0);
  Serial.printf("Temperature: %s C | pH: %s | ORP: %s mV | WiFi: %s\n", t, p, o, wifiText());
}

// ----------------------------------------------------------------- display --

void drawDisplay() {
  char t[12], p[12], o[12], line[24];
  formatValue(t, sizeof(t), temperature, 1);
  formatValue(p, sizeof(p), ph, 2);
  formatValue(o, sizeof(o), orp, 0);

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 10, "WATER MONITOR");
  display.drawHLine(0, 13, 128);
  snprintf(line, sizeof(line), "pH    %s", p);
  display.drawStr(0, 26, line);
  snprintf(line, sizeof(line), "ORP   %s mV", o);
  display.drawStr(0, 38, line);
  snprintf(line, sizeof(line), "TEMP  %s C", t);
  display.drawStr(0, 50, line);
  snprintf(line, sizeof(line), "WiFi  %s", wifiText());
  display.drawStr(0, 62, line);
  display.sendBuffer();
}

// ------------------------------------------------------------------- Wi-Fi --

void startWifi() {
  WiFi.disconnect();
  if (!settings.ssid[0]) return;
  WiFi.begin(settings.ssid, settings.password);
  lastWifiAttemptMs = millis();
}

// Reconnects on its own if the network drops. Never blocks the sensors.
void maintainWifi() {
  if (!settings.ssid[0] || WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttemptMs >= WIFI_RETRY_MS) {
    Serial.println("WiFi: reconnecting");
    startWifi();
  }
}

void appendField(char *body, size_t size, const char *key, const SmoothedReading &r, int decimals) {
  size_t len = strlen(body);
  if (r.has_value()) {
    snprintf(body + len, size - len, ",\"%s\":%.*f", key, decimals, r.value());
  } else {
    snprintf(body + len, size - len, ",\"%s\":null", key);
  }
}

// {"device_id":"aquasense-1","ph":7.42,"orp":681,"temperature":27.4,"rssi":-61,"fw":"1.0.0"}
int postMeasurement() {
  char body[256];
  snprintf(body, sizeof(body), "{\"device_id\":\"%s\"", settings.device_id);
  appendField(body, sizeof(body), "ph", ph, 2);
  appendField(body, sizeof(body), "orp", orp, 0);
  appendField(body, sizeof(body), "temperature", temperature, 2);
  size_t len = strlen(body);
  snprintf(body + len, sizeof(body) - len, ",\"rssi\":%d,\"fw\":\"%s\"}", WiFi.RSSI(),
           AQUASENSE_FW_VERSION);

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure tlsClient;
  bool ok;
  if (strncmp(settings.server, "https://", 8) == 0) {
    // Encrypted, but the server certificate is not checked. Fine for a
    // hobby server; pin your server's CA with tlsClient.setCACert() if needed.
    tlsClient.setInsecure();
    ok = http.begin(tlsClient, settings.server);
  } else {
    ok = http.begin(plainClient, settings.server);
  }
  if (!ok) return -1;
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (settings.token[0]) {
    http.addHeader("Authorization", String("Bearer ") + settings.token);
  }
  int status = http.POST(reinterpret_cast<uint8_t *>(body), strlen(body));
  http.end();
  Serial.printf("POST %s -> %d\n", body, status);
  return status;
}

// ---------------------------------------------------------- serial console --

void printSettings() {
  Serial.printf("ssid      %s\n", settings.ssid[0] ? settings.ssid : "(not set)");
  Serial.printf("password  %s\n", settings.password[0] ? "(set)" : "(not set)");
  Serial.printf("server    %s\n", settings.server[0] ? settings.server : "(not set)");
  Serial.printf("token     %s\n", settings.token[0] ? "(set)" : "(not set)");
  Serial.printf("device    %s\n", settings.device_id);
  Serial.printf("interval  %u s\n", settings.post_interval_s);
  Serial.printf("wifi      %s", wifiText());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" (%s, %d dBm)", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  Serial.printf("\nlast POST %d\n", lastPostStatus);
  Serial.printf("pH board  %s\n", phBoard.present() ? "found at 0x63" : "NOT FOUND");
  Serial.printf("ORP board %s\n", orpBoard.present() ? "found at 0x62" : "NOT FOUND");
  Serial.printf("rejected  temp %lu, pH %lu, ORP %lu\n", temperature.rejected(), ph.rejected(),
                orp.rejected());
}

void printHelp() {
  Serial.println(
      "Setup (saved in flash):\n"
      "  ssid <network name>      Wi-Fi network\n"
      "  password <password>      Wi-Fi password\n"
      "  server <url>             e.g. http://192.168.1.50:8080/api/v1/measurements\n"
      "  token <token>            same as INGEST_TOKEN on the server\n"
      "  device <id>              name for this monitor, e.g. backyard-pool\n"
      "  interval <seconds>       upload interval, 10-3600\n"
      "  show                     print settings and board status\n"
      "  post                     upload once now\n"
      "  quiet / loud             stop / start printing readings\n"
      "  reset                    erase saved settings\n"
      "Calibration (stored on the EZO boards):\n"
      "  cal ph 7 | cal ph 4 | cal ph 10   do 7 first: it clears the others\n"
      "  cal orp 225              ORP to the value printed on your solution\n"
      "  cal ph clear | cal orp clear\n"
      "  cal status\n"
      "Raw EZO commands:\n"
      "  ph <command>             e.g. ph Slope,?\n"
      "  orp <command>            e.g. orp Cal,?");
}

void runEzo(Ezo &board, const char *command) {
  char reply[40];
  int status = board.query(command, reply, sizeof(reply));
  Serial.printf("%s board: %s -> %s %s\n", board.name(), command, Ezo::status_text(status), reply);
  if (status == EZO_OK && strncasecmp(command, "Cal,", 4) == 0) {
    // The window still holds pre-calibration readings.
    if (&board == &phBoard) ph.clear(); else orp.clear();
  }
}

void calibrate(const char *args) {
  char cmd[24];
  float v = 0;
  if (!strcasecmp(args, "status")) {
    runEzo(phBoard, "Cal,?");
    runEzo(orpBoard, "Cal,?");
  } else if (!strcasecmp(args, "ph clear")) {
    runEzo(phBoard, "Cal,clear");
  } else if (!strcasecmp(args, "orp clear")) {
    runEzo(orpBoard, "Cal,clear");
  } else if (sscanf(args, "ph %f", &v) == 1) {
    const char *point = v > 6.5f && v < 7.5f ? "mid" : (v < 7 ? "low" : "high");
    snprintf(cmd, sizeof(cmd), "Cal,%s,%.2f", point, v);
    runEzo(phBoard, cmd);
  } else if (sscanf(args, "orp %f", &v) == 1) {
    snprintf(cmd, sizeof(cmd), "Cal,%.0f", v);
    runEzo(orpBoard, cmd);
  } else {
    Serial.println("Try: cal ph 7, cal ph 4, cal ph 10, cal orp 225, cal status");
  }
}

void setText(char *field, size_t size, const char *value, const char *name) {
  if (strlen(value) >= size) {
    Serial.printf("%s is too long (max %u characters)\n", name, static_cast<unsigned>(size - 1));
    return;
  }
  strcpy(field, value);
  settings.save();
  Serial.printf("%s saved\n", name);
}

void handleCommand(char *line) {
  char *args = strchr(line, ' ');
  if (args) {
    *args++ = '\0';
    while (*args == ' ') args++;
  } else {
    args = line + strlen(line);
  }

  if (!strcasecmp(line, "help")) {
    printHelp();
  } else if (!strcasecmp(line, "show")) {
    printSettings();
  } else if (!strcasecmp(line, "ssid")) {
    setText(settings.ssid, sizeof(settings.ssid), args, "ssid");
    startWifi();
  } else if (!strcasecmp(line, "password")) {
    setText(settings.password, sizeof(settings.password), args, "password");
    startWifi();
  } else if (!strcasecmp(line, "server")) {
    if (args[0] && strncmp(args, "http://", 7) && strncmp(args, "https://", 8)) {
      Serial.println("server must start with http:// or https://");
    } else {
      setText(settings.server, sizeof(settings.server), args, "server");
    }
  } else if (!strcasecmp(line, "token")) {
    setText(settings.token, sizeof(settings.token), args, "token");
  } else if (!strcasecmp(line, "device")) {
    if (!valid_device_id(args)) {
      Serial.println("device id: letters, digits, - _ . only");
    } else {
      setText(settings.device_id, sizeof(settings.device_id), args, "device");
    }
  } else if (!strcasecmp(line, "interval")) {
    int s = atoi(args);
    if (s < 10 || s > 3600) {
      Serial.println("interval must be 10-3600 seconds");
    } else {
      settings.post_interval_s = s;
      settings.save();
      Serial.printf("interval saved: %d s\n", s);
    }
  } else if (!strcasecmp(line, "post")) {
    if (WiFi.status() != WL_CONNECTED || !settings.server[0]) {
      Serial.println("Need Wi-Fi and a server first (see show)");
    } else {
      lastPostStatus = postMeasurement();
      lastPostMs = millis();
    }
  } else if (!strcasecmp(line, "quiet")) {
    printReadings = false;
  } else if (!strcasecmp(line, "loud")) {
    printReadings = true;
  } else if (!strcasecmp(line, "reset")) {
    settings.clear();
    startWifi();
    Serial.println("settings erased");
  } else if (!strcasecmp(line, "cal")) {
    calibrate(args);
  } else if (!strcasecmp(line, "ph")) {
    runEzo(phBoard, args);
  } else if (!strcasecmp(line, "orp")) {
    runEzo(orpBoard, args);
  } else if (line[0]) {
    Serial.printf("Unknown command '%s'. Type help.\n", line);
  }
}

void handleSerial() {
  static char line[200];
  static size_t len = 0;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      line[len] = '\0';
      handleCommand(line);
      len = 0;
    } else if (len + 1 < sizeof(line)) {
      line[len++] = c;
    }
  }
}

// --------------------------------------------------------------- main loop --

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nAquaSense %s\n", AQUASENSE_FW_VERSION);

  settings.load();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
  display.setBusClock(I2C_CLOCK_HZ);
  display.begin();

  tempProbe.begin();
  tempProbe.setResolution(12);

  Serial.printf("pH board:  %s\n", phBoard.present() ? "found" : "NOT FOUND at 0x63");
  Serial.printf("ORP board: %s\n", orpBoard.present() ? "found" : "NOT FOUND at 0x62");
  Serial.printf("Temperature probes: %d\n", tempProbe.getDeviceCount());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  startWifi();

  Serial.println("Type help for commands.");
}

void loop() {
  handleSerial();
  maintainWifi();

  uint32_t now = millis();
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = now;
    readSensors();
    drawDisplay();
    if (printReadings) printToSerial();
  }

  bool anyValue = ph.has_value() || orp.has_value() || temperature.has_value();
  if (settings.server[0] && WiFi.status() == WL_CONNECTED && anyValue &&
      (lastPostMs == 0 || millis() - lastPostMs >= settings.post_interval_s * 1000UL)) {
    lastPostMs = millis();
    lastPostStatus = postMeasurement();
  }
}
