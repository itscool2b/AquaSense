// Layer 2: DS18B20 waterproof temperature probe.
//
//   DS18B20 red    -> 3.3V
//   DS18B20 black  -> GND
//   DS18B20 yellow -> GPIO 4   (some probes use white for data)
//   4.7 kOhm resistor between GPIO 4 and 3.3V
//
// Libraries: OneWire, DallasTemperature.

#include <DallasTemperature.h>
#include <OneWire.h>

#define TEMP_PIN 4

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  Serial.print("Probes found: ");
  Serial.println(sensors.getDeviceCount());
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Temperature: probe not found (check wiring and the 4.7k resistor)");
  } else {
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println(" C");
  }

  delay(2000);
}
