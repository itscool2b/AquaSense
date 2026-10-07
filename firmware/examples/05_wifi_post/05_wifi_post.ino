// Layer 8 check: can the ESP32 reach your server?
//
// Fill in the four values below, upload, and open the Serial Monitor.
// It joins Wi-Fi and POSTs one test measurement every 10 s:
//   {"device_id":"wifi-test","ph":7.42,"orp":681,"temperature":27.4}
// A 200 means the row is in your database. Then go back to the full
// firmware, where these settings are typed in at runtime instead.

#include <HTTPClient.h>
#include <WiFi.h>

const char *ssid = "YOUR_WIFI";
const char *password = "YOUR_PASSWORD";
const char *server = "http://192.168.1.50:8080/api/v1/measurements";
const char *token = "change-me";  // INGEST_TOKEN from selfhost/.env

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected, IP ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(server);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + token);
    int status = http.POST("{\"device_id\":\"wifi-test\",\"ph\":7.42,\"orp\":681,\"temperature\":27.4}");
    Serial.printf("POST -> %d %s\n", status, http.getString().c_str());
    http.end();
  } else {
    Serial.println("Wi-Fi dropped, reconnecting");
    WiFi.reconnect();
  }
  delay(10000);
}
