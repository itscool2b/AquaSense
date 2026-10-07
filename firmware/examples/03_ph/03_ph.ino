// Layer 3: pH probe -> isolated EZO-pH board -> ESP32 over I2C.
//
// The EZO board must already be in I2C mode (address 99 / 0x63).
//   Carrier VCC -> 3.3V     Carrier GND -> GND
//   Carrier TX  -> GPIO 21 (SDA)   Carrier RX -> GPIO 22 (SCL)
//
// Prints a reading every 2 s. Anything you type in the Serial Monitor is
// sent to the board as a command, e.g.
//   Cal,mid,7.00   Cal,low,4.00   Cal,high,10.00   Cal,?   Slope,?
// Use "Newline" as the line ending.

#include <Wire.h>

#define PH_ADDRESS 0x63

// Sends a command, waits, and prints/returns the answer.
// Reply code: 1 ok, 2 syntax error, 254 still processing, 255 no data.
int ezoCommand(const char *cmd, char *reply, size_t size) {
  Wire.beginTransmission(PH_ADDRESS);
  Wire.write(cmd);
  if (Wire.endTransmission() != 0) {
    reply[0] = '\0';
    return 0;  // nothing answered
  }
  bool slow = (cmd[0] == 'R' || cmd[0] == 'r') || (strncasecmp(cmd, "Cal,", 4) == 0 && cmd[4] != '?');
  delay(slow ? 900 : 300);

  Wire.requestFrom(PH_ADDRESS, 40);
  int code = Wire.read();
  size_t n = 0;
  while (Wire.available()) {
    char c = Wire.read();
    if (c == '\0') break;
    if (n + 1 < size) reply[n++] = c;
  }
  reply[n] = '\0';
  return code;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.beginTransmission(PH_ADDRESS);
  if (Wire.endTransmission() == 0) {
    Serial.println("pH board found at 0x63");
  } else {
    Serial.println("pH board NOT found. Check wiring and that it is in I2C mode.");
  }
}

void loop() {
  static String typed;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      typed.trim();
      if (typed.length()) {
        char reply[40];
        int code = ezoCommand(typed.c_str(), reply, sizeof(reply));
        Serial.printf(">> %s -> code %d %s\n", typed.c_str(), code, reply);
      }
      typed = "";
    } else if (c != '\r') {
      typed += c;
    }
  }

  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    char reply[40];
    if (ezoCommand("R", reply, sizeof(reply)) == 1) {
      Serial.print("pH: ");
      Serial.println(reply);
    } else {
      Serial.println("pH: no reading");
    }
  }
}
