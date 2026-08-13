/*
 * AquaSense — a solar buoy that sits on the water, measures a few stats,
 * uses cellular internet to send them to a website you host.
 *
 * Board: LilyGO T-A7670G R2 with GPS.
 * Pin map: hardware/pinmap.md (must match this file).
 * HTTPS: TinyGSM-fork (lewisxhe), not mainline TinyGSM.
 * ADC: analogReadMilliVolts(), not Uno 1024*5000.
 */

#include <Arduino.h>
#include <math.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DFRobot_PH.h>
#include <DFRobot_EC10.h>
#include <SparkFun_MS5803_I2C.h>
#include <TinyGsmClient.h>

#include "pins.h"
#include "convert.h"

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature dallas(&oneWire);
DFRobot_PH phSensor;
DFRobot_EC10 ecSensor;
MS5803 depthSensor(ADDRESS_HIGH);
Preferences prefs;

TinyGsm modem(SerialAT);

String ingestUrl = "http://192.168.1.10:8080/api/v1/ingest";
String deviceId = "buoy-01";
String token = "change-me";
String apn = "iot.1nce.net";
uint32_t samplePeriodS = SAMPLE_PERIOD_S;
float pAtmMbar = 1013.25f;
bool stayAwake = false;

float doMvZero = NAN;
float doMvAir = NAN;

static int cmpFloat(const void *a, const void *b) {
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

static float medianMilliVolts(int pin) {
  float buf[ADC_SAMPLES];
  for (int i = 0; i < ADC_SAMPLES; i++) {
    buf[i] = (float)analogReadMilliVolts(pin);
    delay(20);
  }
  qsort(buf, ADC_SAMPLES, sizeof(float), cmpFloat);
  return buf[ADC_SAMPLES / 2];
}

static void loadDoCal() {
  prefs.begin("aquasense", true);
  if (prefs.getBool("do_ok", false)) {
    doMvZero = prefs.getFloat("do_z", NAN);
    doMvAir = prefs.getFloat("do_a", NAN);
  } else {
    doMvZero = NAN;
    doMvAir = NAN;
  }
  prefs.end();
}

static void saveDoCal() {
  prefs.begin("aquasense", false);
  prefs.putFloat("do_z", doMvZero);
  prefs.putFloat("do_a", doMvAir);
  prefs.putBool("do_ok", isfinite(doMvZero) && isfinite(doMvAir) &&
                             (doMvAir > doMvZero + 10.0f));
  prefs.end();
}

static bool loadConfig() {
  File f = SD.open("/config.json", FILE_READ);
  if (!f) {
    Serial.println("No /config.json on SD — using firmware defaults.");
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.println("config.json parse failed.");
    return false;
  }
  if (doc["ingest_url"].is<const char *>()) {
    ingestUrl = String(doc["ingest_url"].as<const char *>());
  }
  if (doc["device_id"].is<const char *>()) {
    deviceId = String(doc["device_id"].as<const char *>());
  }
  if (doc["token"].is<const char *>()) {
    token = String(doc["token"].as<const char *>());
  }
  if (doc["apn"].is<const char *>()) {
    apn = String(doc["apn"].as<const char *>());
  }
  if (doc["sample_period_s"].is<uint32_t>()) {
    samplePeriodS = doc["sample_period_s"].as<uint32_t>();
  }
  if (doc["p_atm_mbar"].is<float>()) {
    pAtmMbar = doc["p_atm_mbar"].as<float>();
  }
  if (doc["stay_awake"].is<bool>()) {
    stayAwake = doc["stay_awake"].as<bool>();
  }
  return true;
}

static void appendCsv(const char *line) {
  File f = SD.open("/readings.csv", FILE_APPEND);
  if (!f) {
    f = SD.open("/readings.csv", FILE_WRITE);
  }
  if (!f) {
    return;
  }
  f.println(line);
  f.close();
}

static double nmeaToDegrees(const String &dm, const String &hemi) {
  if (dm.length() < 4) {
    return 0;
  }
  const double v = dm.toDouble();
  const int deg = (int)(v / 100.0);
  const double minutes = v - (deg * 100.0);
  double out = deg + (minutes / 60.0);
  if (hemi == "S" || hemi == "W") {
    out = -out;
  }
  return out;
}

static bool parseGga(const String &nmea, double *lat, double *lon) {
  int idx = nmea.lastIndexOf("$GNGGA");
  if (idx < 0) {
    idx = nmea.lastIndexOf("$GPGGA");
  }
  if (idx < 0) {
    return false;
  }
  String line = nmea.substring(idx);
  int end = line.indexOf('\n');
  if (end > 0) {
    line = line.substring(0, end);
  }
  String parts[8];
  int start = 0;
  int n = 0;
  for (unsigned i = 0; i < line.length() && n < 8; i++) {
    if (line[i] == ',') {
      parts[n++] = line.substring(start, i);
      start = i + 1;
    }
  }
  if (n < 6 || parts[2].length() == 0 || parts[4].length() == 0) {
    return false;
  }
  *lat = nmeaToDegrees(parts[2], parts[3]);
  *lon = nmeaToDegrees(parts[4], parts[5]);
  return true;
}

static bool postReading(const char *json) {
  if (!modem.https_begin()) {
    Serial.println("https_begin failed");
    return false;
  }
  if (!modem.https_set_url(ingestUrl.c_str())) {
    Serial.println("https_set_url failed");
    modem.https_end();
    return false;
  }
  modem.https_set_content_type("application/json");
  String auth = String("Bearer ") + token;
  modem.https_add_header("Authorization", auth.c_str());
  int code = modem.https_post(json);
  Serial.printf("HTTPS POST status %d\n", code);
  modem.https_end();
  return code >= 200 && code < 300;
}

static void pulseModemPower() {
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
}

static float readTempC() {
  dallas.requestTemperatures();
  float tempC = dallas.getTempCByIndex(0);
  if (tempC < -50) {
    tempC = 25.0f;
  }
  return tempC;
}

static float batteryVolts() {
  /* LilyGO ReadBattery.ino (V1.2+): 100 kΩ / 100 kΩ on GPIO 35, so
     analogReadMilliVolts() * 2. ADC_11db as in that example.
     https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/examples/Arduino_Devices_Testing/ReadBattery/ReadBattery.ino
     V1.1 has no divider — do not use ×2 on V1.1. Issue #33 measured 100k/100k
     in-circuit on later revs. */
  int batRaw = analogReadMilliVolts(BOARD_BAT_ADC_PIN);
  return batRaw / 1000.0f * 2.0f;
}

static void printSample(float tempC, float phMv, float ecMv, float doMv, float ph,
                        float spcond, float doMgl, float doSat, float doPct) {
  Serial.printf("T=%.2f C  pH_mV=%.0f pH=%.2f  EC_mV=%.0f mS/cm=%.2f\n", tempC, phMv,
                ph, ecMv, spcond);
  Serial.printf("DO_mV=%.0f  do_mgl=", doMv);
  if (isnan(doMgl)) {
    Serial.print("null (uncalibrated — doair + dozero)");
  } else {
    Serial.printf("%.2f", doMgl);
  }
  Serial.printf("  sat=%.2f  do_pct=", doSat);
  if (isnan(doPct)) {
    Serial.println("null");
  } else {
    Serial.printf("%.1f\n", doPct);
  }
}

static String readSerialLine() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      String out = buf;
      buf = "";
      out.trim();
      if (out.length()) {
        return out;
      }
    } else if (buf.length() < 32) {
      buf += c;
    }
  }
  return "";
}

static void runCalMode() {
  Serial.println("CAL MODE — board stays awake. USB serial, 115200, NL.");
  Serial.println("DO:  doair (air-sat water, pump ~20 min)  dozero (zero solution)");
  Serial.println("     dostatus   sample");
  Serial.println("pH:  enterph  calph  exitph     EC:  enterec  calec  exitec");
  Serial.println("sleep — one POST, then deep-sleep");

  while (true) {
    float tempC = readTempC();
    float phMv = medianMilliVolts(PIN_PH_ADC);
    float ecMv = medianMilliVolts(PIN_EC_ADC);
    float doMv = medianMilliVolts(PIN_DO_ADC);
    /* EC calec uses _ecvalueRaw from the last readEC. */
    float spcond = ecSensor.readEC(ecMv, tempC);
    float ph = phSensor.readPH(phMv, tempC);
    float sal = (float)aquasense_salinity_psu(spcond);
    float doSat = (float)aquasense_do_sat_mgl(tempC, isnan(sal) ? 0 : sal, 1.0);
    float doMgl = (float)aquasense_do_mgl_from_mv(doMv, doMvZero, doMvAir, doSat);
    float doPct = (float)aquasense_do_percent(doMgl, doSat);

    String cmd = readSerialLine();
    if (cmd.length()) {
      String lower = cmd;
      lower.toLowerCase();
      if (lower == "sleep") {
        Serial.println("Leaving CAL MODE.");
        return;
      }
      if (lower == "doair") {
        doMvAir = doMv;
        saveDoCal();
        Serial.printf("Stored DO air-sat mV = %.1f (sat %.2f mg/L at %.1f C)\n", doMvAir,
                      doSat, tempC);
      } else if (lower == "dozero") {
        doMvZero = doMv;
        saveDoCal();
        Serial.printf("Stored DO zero mV = %.1f\n", doMvZero);
      } else if (lower == "dostatus") {
        Serial.printf("DO cal: zero_mV=");
        if (isnan(doMvZero)) {
          Serial.print("unset");
        } else {
          Serial.printf("%.1f", doMvZero);
        }
        Serial.print("  air_mV=");
        if (isnan(doMvAir)) {
          Serial.println("unset");
        } else {
          Serial.printf("%.1f\n", doMvAir);
        }
      } else if (lower == "sample") {
        printSample(tempC, phMv, ecMv, doMv, ph, spcond, doMgl, doSat, doPct);
      } else {
        /* DFRobot EC strupr walks until a space — keep a trailing one. */
        char buf[16];
        snprintf(buf, sizeof(buf), "%s ", lower.c_str());
        phSensor.calibration(phMv, tempC, buf);
        snprintf(buf, sizeof(buf), "%s ", lower.c_str());
        ecSensor.calibration(ecMv, tempC, buf);
        EEPROM.commit();
      }
    }
    delay(200);
  }
}

static bool waitForCal() {
  if (stayAwake) {
    Serial.println("stay_awake in config.json — entering CAL MODE.");
    return true;
  }
  Serial.println("Type cal within 12s to stay awake for calibration.");
  unsigned long t0 = millis();
  while (millis() - t0 < 12000) {
    String cmd = readSerialLine();
    if (cmd.length()) {
      cmd.toLowerCase();
      if (cmd == "cal") {
        return true;
      }
    }
    delay(50);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("AquaSense boot");
  Serial.println("A solar buoy that sits on the water, measures a few stats,");
  Serial.println("uses cellular internet to send them to a website you host.");

  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  EEPROM.begin(64);
  loadDoCal();

  SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN, BOARD_SD_CS_PIN);
  if (!SD.begin(BOARD_SD_CS_PIN)) {
    Serial.println("SD init failed — continuing with defaults. Remove SD before USB upload.");
  } else {
    loadConfig();
  }

  dallas.begin();
  phSensor.begin();
  ecSensor.begin();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  depthSensor.reset();
  depthSensor.begin();

  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  pulseModemPower();
  delay(3000);
  if (!modem.init()) {
    Serial.println("modem.init failed — will retry after sleep");
  } else {
    modem.gprsConnect(apn.c_str());
  }

  Serial2.begin(9600, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  pinMode(GNSS_WAKE_PIN, OUTPUT);
  digitalWrite(GNSS_WAKE_PIN, HIGH);

  if (waitForCal()) {
    runCalMode();
  }
}

void loop() {
  delay(SENSOR_WARMUP_MS);

  float tempC = readTempC();

  float phMv = medianMilliVolts(PIN_PH_ADC);
  float ecMv = medianMilliVolts(PIN_EC_ADC);
  float doMv = medianMilliVolts(PIN_DO_ADC);

  float ph = phSensor.readPH(phMv, tempC);
  float spcond = ecSensor.readEC(ecMv, tempC);
  float sal = (float)aquasense_salinity_psu(spcond);
  float doSat = (float)aquasense_do_sat_mgl(tempC, isnan(sal) ? 0 : sal, 1.0);
  float doMgl = (float)aquasense_do_mgl_from_mv(doMv, doMvZero, doMvAir, doSat);
  float doPct = (float)aquasense_do_percent(doMgl, doSat);

  float pMbar = (float)depthSensor.getPressure(ADC_4096);
  float depthM = (float)aquasense_depth_m(pMbar, pAtmMbar);
  float battV = batteryVolts();

  double lat = 0, lon = 0;
  String nmea;
  while (Serial2.available()) {
    nmea += (char)Serial2.read();
    if (nmea.length() > 512) {
      nmea.remove(0, nmea.length() - 256);
    }
  }
  parseGga(nmea, &lat, &lon);

  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["token"] = token;
  doc["ts"] = (uint32_t)(millis() / 1000);
  doc["lat"] = lat;
  doc["lon"] = lon;
  doc["temp_c"] = tempC;
  doc["ph"] = ph;
  doc["spcond_ms_cm"] = spcond;
  doc["sal_psu"] = sal;
  if (isnan(doMgl)) {
    doc["do_mgl"] = nullptr;
  } else {
    doc["do_mgl"] = doMgl;
  }
  if (isnan(doPct)) {
    doc["do_pct"] = nullptr;
  } else {
    doc["do_pct"] = doPct;
  }
  doc["depth_m"] = depthM;
  doc["batt_v"] = battV;
  doc["rssi"] = modem.getSignalQuality();
  doc["fw"] = "0.2.0";

  char json[768];
  size_t n = serializeJson(doc, json, sizeof(json));
  json[n] = 0;
  Serial.println(json);
  appendCsv(json);
  postReading(json);

  Serial.printf("Sleep %u s\n", samplePeriodS);
  modem.https_end();
  esp_sleep_enable_timer_wakeup((uint64_t)samplePeriodS * 1000000ULL);
  esp_deep_sleep_start();
}
