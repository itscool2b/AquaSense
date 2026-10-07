#pragma once

#include <Arduino.h>

/*
 * Settings typed into the serial monitor, saved in the ESP32's flash
 * (Preferences), so they survive power loss and never live in the code.
 *
 * pH and ORP calibration is stored on the EZO boards themselves, which keep
 * it through power loss too.
 */
struct Settings {
  char ssid[33] = "";
  char password[65] = "";
  char server[161] = "";  // e.g. http://192.168.1.50:8080/api/v1/measurements
  char token[65] = "";
  char device_id[33] = "aquasense-1";
  uint16_t post_interval_s = 60;

  void load();
  void save() const;
  void clear();
};

// Keeps device ids JSON- and URL-safe: letters, digits, '-', '_' and '.'.
bool valid_device_id(const char *id);
