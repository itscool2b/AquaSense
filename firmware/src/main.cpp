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

TinyGsm modem(SerialAT);

String ingestUrl = "http://192.168.1.10:8080/api/v1/ingest";
String deviceId = "buoy-01";
String token = "change-me";
String apn = "iot.1nce.net";
uint32_t samplePeriodS = SAMPLE_PERIOD_S;
float pAtmMbar = 1013.25f;

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

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("AquaSense boot");
  Serial.println("A solar buoy that sits on the water, measures a few stats,");
  Serial.println("uses cellular internet to send them to a website you host.");

  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);

  analogReadResolution(12);

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
}

void loop() {
  delay(SENSOR_WARMUP_MS);

  dallas.requestTemperatures();
  float tempC = dallas.getTempCByIndex(0);
  if (tempC < -50) {
    tempC = 25.0f;
  }

  float phMv = medianMilliVolts(PIN_PH_ADC);
  float ecMv = medianMilliVolts(PIN_EC_ADC);
  float doMv = medianMilliVolts(PIN_DO_ADC);

  float ph = phSensor.readPH(phMv, tempC);
  float spcond = ecSensor.readEC(ecMv, tempC);
  /* SEN0237 two-point is stored by the DFRobot example as linear mV→mg/L.
     Until calibrated, report millivolts-derived placeholder via 0–20 mg/L
     over 0–3000 mV (datasheet analog span 0–3.0 V). Calibrate on the desk. */
  float doMgl = (doMv / 3000.0f) * 20.0f;
  if (doMgl < 0) {
    doMgl = 0;
  }

  float sal = (float)aquasense_salinity_psu(spcond);
  float doSat = (float)aquasense_do_sat_mgl(tempC, isnan(sal) ? 0 : sal, 1.0);
  float doPct = (float)aquasense_do_percent(doMgl, doSat);

  float pMbar = (float)depthSensor.getPressure(ADC_4096);
  float depthM = (float)aquasense_depth_m(pMbar, pAtmMbar);

  int batRaw = analogReadMilliVolts(BOARD_BAT_ADC_PIN);
  /* LilyGO battery ADC is a divider; millivolts × 2 is the usual 1:1 pad
     until you measure the exact ratio on your board. */
  float battV = batRaw / 1000.0f * 2.0f;

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
  doc["do_mgl"] = doMgl;
  doc["do_pct"] = doPct;
  doc["depth_m"] = depthM;
  doc["batt_v"] = battV;
  doc["rssi"] = modem.getSignalQuality();
  doc["fw"] = "0.1.0";

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
