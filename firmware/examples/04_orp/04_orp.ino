// Layer 4: ORP probe -> isolated EZO-ORP board -> same I2C bus as pH.
//
// The EZO-ORP board must already be in I2C mode (address 98 / 0x62).
// Wire it exactly like the pH carrier board: VCC 3.3V, GND, TX->21, RX->22.
//
// Prints pH and ORP every 2 s. Type "orp <command>" or "ph <command>" to
// talk to a board, e.g.  orp Cal,225   orp Cal,?   ph Cal,?
// Use "Newline" as the line ending.

#include <Wire.h>

#define PH_ADDRESS 0x63
#define ORP_ADDRESS 0x62

int ezoCommand(uint8_t address, const char *cmd, char *reply, size_t size) {
  Wire.beginTransmission(address);
  Wire.write(cmd);
  if (Wire.endTransmission() != 0) {
    reply[0] = '\0';
    return 0;
  }
  bool slow = (cmd[0] == 'R' || cmd[0] == 'r') || (strncasecmp(cmd, "Cal,", 4) == 0 && cmd[4] != '?');
  delay(slow ? 900 : 300);

  Wire.requestFrom(address, (uint8_t)40);
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

void report(const char *name, uint8_t address) {
  Wire.beginTransmission(address);
  Serial.printf("%s board %s\n", name, Wire.endTransmission() == 0 ? "found" : "NOT found");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  report("pH", PH_ADDRESS);
  report("ORP", ORP_ADDRESS);
}

void loop() {
  static String typed;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      typed.trim();
      int space = typed.indexOf(' ');
      if (space > 0) {
        String who = typed.substring(0, space);
        String cmd = typed.substring(space + 1);
        uint8_t address = who.equalsIgnoreCase("orp") ? ORP_ADDRESS : PH_ADDRESS;
        char reply[40];
        int code = ezoCommand(address, cmd.c_str(), reply, sizeof(reply));
        Serial.printf(">> %s %s -> code %d %s\n", who.c_str(), cmd.c_str(), code, reply);
      }
      typed = "";
    } else if (c != '\r') {
      typed += c;
    }
  }

  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    char phReply[40], orpReply[40];
    int phCode = ezoCommand(PH_ADDRESS, "R", phReply, sizeof(phReply));
    int orpCode = ezoCommand(ORP_ADDRESS, "R", orpReply, sizeof(orpReply));
    Serial.printf("pH: %s | ORP: %s mV\n", phCode == 1 ? phReply : "--", orpCode == 1 ? orpReply : "--");
  }
}
