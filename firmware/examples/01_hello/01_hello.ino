// Layer 1: prove the ESP32, the USB cable and the Arduino IDE all work.
// Nothing else connected. Serial Monitor at 115200 baud.

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println("System running");
  delay(1000);
}
