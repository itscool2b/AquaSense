#include "ezo.h"

#include <Wire.h>

static const size_t kReplyBytes = 40;

bool Ezo::send(const char *command) {
  Wire.beginTransmission(address_);
  Wire.write(reinterpret_cast<const uint8_t *>(command), strlen(command));
  return Wire.endTransmission() == 0;
}

int Ezo::read(char *reply, size_t size) {
  if (size) reply[0] = '\0';
  if (Wire.requestFrom(address_, static_cast<uint8_t>(kReplyBytes)) == 0) {
    return EZO_NO_DEVICE;
  }
  int code = Wire.read();
  size_t n = 0;
  while (Wire.available()) {
    char c = static_cast<char>(Wire.read());
    if (c == '\0') break;
    if (n + 1 < size) reply[n++] = c;
  }
  while (Wire.available()) Wire.read();
  if (size) reply[n] = '\0';
  return code;
}

int Ezo::query(const char *command, char *reply, size_t size) {
  if (!send(command)) {
    if (size) reply[0] = '\0';
    return EZO_NO_DEVICE;
  }
  delay(processing_delay_ms(command));
  return read(reply, size);
}

float Ezo::reading() {
  char reply[kReplyBytes];
  if (query("R", reply, sizeof(reply)) != EZO_OK || reply[0] == '\0') {
    return NAN;
  }
  return atof(reply);
}

bool Ezo::present() {
  Wire.beginTransmission(address_);
  return Wire.endTransmission() == 0;
}

uint16_t Ezo::processing_delay_ms(const char *command) {
  char c0 = toupper(command[0]);
  char c1 = toupper(command[1]);
  // R, RT,n and Cal,... need 900 ms; T,n, Cal,? and the rest need 300 ms.
  if (c0 == 'R' && (c1 == '\0' || c1 == 'T')) return 900;
  if (strncasecmp(command, "Cal,", 4) == 0 && command[4] != '?') return 900;
  return 300;
}

const char *Ezo::status_text(int status) {
  switch (status) {
    case EZO_OK: return "ok";
    case EZO_SYNTAX_ERROR: return "syntax error";
    case EZO_PENDING: return "still processing";
    case EZO_NO_DATA: return "no data";
    case EZO_NO_DEVICE: return "no answer (check wiring and I2C mode)";
    default: return "unexpected reply";
  }
}
