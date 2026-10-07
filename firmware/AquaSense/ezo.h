#pragma once

#include <Arduino.h>

/*
 * Atlas Scientific EZO circuits (EZO-pH, EZO-ORP) over I2C.
 *
 * Protocol (EZO datasheets, "I2C mode"): write the command as ASCII, wait the
 * processing delay, then read. The first byte of the reply is a code:
 *   1 = success, 2 = syntax error, 254 = still processing, 255 = no data.
 * The text answer follows and ends with a null byte.
 *
 * The boards ship in UART mode. Switch each one to I2C once before use
 * (see the pH step of the build guide).
 */

enum EzoStatus : int {
  EZO_NO_DEVICE = 0,  // nothing answered at this address
  EZO_OK = 1,
  EZO_SYNTAX_ERROR = 2,
  EZO_PENDING = 254,
  EZO_NO_DATA = 255,
};

class Ezo {
 public:
  Ezo(uint8_t address, const char *name) : address_(address), name_(name) {}

  // Sends a command without waiting for the answer.
  bool send(const char *command);

  // Reads the answer to the last command into `reply`.
  int read(char *reply, size_t size);

  // Sends, waits the processing delay for that command, then reads.
  int query(const char *command, char *reply, size_t size);

  // Sends "R" and parses the number. Returns NaN on any failure.
  float reading();

  bool present();
  uint8_t address() const { return address_; }
  const char *name() const { return name_; }

  // 900 ms for readings and calibration, 300 ms for everything else.
  static uint16_t processing_delay_ms(const char *command);
  static const char *status_text(int status);

 private:
  uint8_t address_;
  const char *name_;
};
