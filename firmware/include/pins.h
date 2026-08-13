#pragma once

/* LilyGO T-A7670G R2 with GPS.
 * https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/docs/en/esp32/a7670-esp32/README.MD
 */

#define LILYGO_T_A7670

#define MODEM_BAUDRATE 115200
#define MODEM_DTR_PIN 25
#define MODEM_TX_PIN 26
#define MODEM_RX_PIN 27
#define BOARD_PWRKEY_PIN 4
#define BOARD_POWERON_PIN 12
#define MODEM_RING_PIN 33
#define MODEM_RESET_PIN 5
#define MODEM_RESET_LEVEL HIGH
#define BOARD_MISO_PIN 2
#define BOARD_MOSI_PIN 15
#define BOARD_SCK_PIN 14
#define BOARD_SD_CS_PIN 13
#define BOARD_BAT_ADC_PIN 35
#define BOARD_SOLAR_ADC_PIN 36
#define SerialAT Serial1

#define GNSS_TX_PIN 21
#define GNSS_RX_PIN 22
#define GNSS_PPS_PIN 23
#define GNSS_WAKE_PIN 19

#define PIN_PH_ADC 32
#define PIN_EC_ADC 34
#define PIN_DO_ADC 39
#define PIN_ONEWIRE 18
#define PIN_I2C_SDA 16
#define PIN_I2C_SCL 17

#define SAMPLE_PERIOD_S 900
#define SENSOR_WARMUP_MS 8000
#define ADC_SAMPLES 11

#ifndef TINY_GSM_MODEM_A7670
#define TINY_GSM_MODEM_A7670
#endif
