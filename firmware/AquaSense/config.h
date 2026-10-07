#pragma once

/*
 * AquaSense build settings. Pin numbers match hardware/pinmap.md and the
 * wiring diagram in hardware/diagrams/wiring.svg.
 *
 * Board: any ESP32 DevKit (ESP32-WROOM-32). ESP32 GPIO logic is 3.3 V.
 */

#define AQUASENSE_FW_VERSION "1.0.0"

// DS18B20 waterproof temperature probe. 4.7 kOhm pull-up from GPIO 4 to 3.3 V.
#define PIN_ONEWIRE 4

// One I2C bus shared by the pH board, the ORP board and the OLED display.
// In I2C mode an EZO board's TX pin is SDA and its RX pin is SCL.
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_CLOCK_HZ 100000

// Atlas Scientific EZO factory I2C addresses.
#define EZO_PH_ADDR 0x63   // 99
#define EZO_ORP_ADDR 0x62  // 98

// OLED controller. Uncomment one if yours is not an SSD1306 (most 0.96" boards).
// #define AQUASENSE_OLED_SH1106    // most 1.3" boards
// #define AQUASENSE_OLED_SSD1309   // most 2.42" boards

// One reading of all three sensors every 2 s. The display and serial
// output show the average of the last 10 accepted readings (~20 s).
#define READ_INTERVAL_MS 2000

// Default upload interval. Change it at runtime with "interval <seconds>".
#define DEFAULT_POST_INTERVAL_S 60

// Retry Wi-Fi this often while disconnected.
#define WIFI_RETRY_MS 15000
