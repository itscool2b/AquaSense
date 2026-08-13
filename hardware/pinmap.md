# Pin map — LilyGO T-A7670G R2 **with GPS**

Source: [LilyGO A7670 ESP32 README](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series/blob/main/docs/en/esp32/a7670-esp32/README.MD)
(retrieved 2026-08-12). Firmware `#define`s, the wiring SVG, and this table are the same.

The GPS variant uses GPIO 21/22 for the external L76K GNSS UART. **Do not use 21/22 as I2C.**

Do **not** put jumper wires on header GPIO 4 (modem PWRKEY) or GPIO 5 (modem RST).

| Function | GPIO | Notes |
|---|---|---|
| Peripheral power (SD + modem) | 12 | Must be HIGH after boot or the board resets on battery |
| Modem TX | 26 | Do not reuse |
| Modem RX | 27 | Do not reuse |
| Modem PWRKEY | 4 | Header present — leave unconnected |
| Modem RESET | 5 | Header present — leave unconnected |
| Modem DTR | 25 | Sleep control |
| Modem RING | 33 | Input only |
| SD SCK / MISO / MOSI / CS | 14 / 2 / 15 / 13 | Remove SD card before USB upload (IO2) |
| Battery ADC | 35 | On-board divider |
| Solar ADC | 36 | V1.4+ |
| GNSS TX / RX (GPS SKU only) | 21 / 22 | L76K module |
| GNSS PPS / Wake | 23 / 19 | GPS SKU only |
| **pH analog (SEN0169-V2)** | **32** | ADC1, 0–3.0 V transmitter |
| **EC analog (DFR0300-H)** | **34** | ADC1 input-only, 0–3.2 V transmitter |
| **DO analog (SEN0237-A)** | **39** | ADC1 input-only, 0–3.0 V transmitter |
| **DS18B20 1-Wire** | **18** | Gravity kit includes pull-up |
| **MS5803 SDA** | **16** | I2C bit-bang / Wire on 16/17 |
| **MS5803 SCL** | **17** | |
| Analog transmitter 3.3 V | 3V3 | Shared rail. Transmitters stay powered (no MOSFET in the default build). |
| Analog / DS18B20 GND | GND | Common ground |

ESP32 analog math is 12-bit. Use `analogReadMilliVolts()` (Arduino-ESP32), not Uno `analogRead()/1024.0*5000`.
