# AquaSense firmware

ESP32 firmware for continuous pH, ORP and temperature:

```
pH probe  -> isolated EZO-pH board  --\
ORP probe -> isolated EZO-ORP board ---+-- I2C (GPIO 21/22) --> ESP32 --> OLED display
DS18B20 temperature probe -- GPIO 4 --/                            |
                                                       Wi-Fi --> your server + dashboard
```

| Folder | What it is |
|---|---|
| [`examples/`](examples/) | One small sketch per build layer. Test each part before adding the next. |
| [`AquaSense/`](AquaSense/) | The full firmware. Opens directly in the Arduino IDE. |
| [`test/`](test/) | Computer-side tests for the averaging and bad-reading filter. |

## Layer sketches

| Sketch | Build step | You should see |
|---|---|---|
| `01_hello` | ESP32 alone | `System running` every second |
| `02_temperature` | + DS18B20 | `Temperature: 25.63 C` |
| `03_ph` | + EZO-pH | `pH: 7.01` in pH 7 buffer; type `Cal,mid,7.00` to calibrate |
| `04_orp` | + EZO-ORP | `pH: 7.01 \| ORP: 225 mV`; type `orp Cal,225` |
| `05_wifi_post` | Wi-Fi + server | `POST -> 200` and a row in your database |

## Full firmware

What it does:

- Reads temperature, then sends it to the pH board (`T,n`) so pH is temperature-compensated.
- Reads pH and ORP every 2 s and averages the last 10 good readings.
- Rejects impossible values (pH outside 0–14, temperature outside −10–80 °C, an unplugged probe)
  and sudden jumps such as `7.41, 7.40, 12.98, 7.41`. Three jumps in a row that agree are treated as a
  real change, so moving the probe into another buffer still works.
- Shows pH, ORP, temperature and Wi-Fi status on a 128×64 I²C OLED (optional).
- Joins Wi-Fi without blocking the sensors, reconnects on its own, and POSTs every 60 s:

```json
{"device_id":"backyard-pool","ph":7.42,"orp":681,"temperature":27.4,"rssi":-61,"fw":"1.0.0"}
```

A sensor that stops answering is sent as `null`, never as a made-up number.

### Arduino IDE

1. Install ESP32 board support (Boards Manager → "esp32 by Espressif Systems").
2. Install libraries: **OneWire**, **DallasTemperature**, **U8g2**.
3. Open `AquaSense/AquaSense.ino`. Board: **ESP32 Dev Module**. Upload.
4. Serial Monitor at 115200 baud, line ending **Newline**. Type `help`.

### PlatformIO

```bash
cd firmware
pio run -t upload
pio device monitor
make test            # filter tests on your computer, no ESP32 needed
```

### Setup commands (saved in the ESP32's flash)

```
ssid My Home WiFi
password your-wifi-password
server http://192.168.1.50:8080/api/v1/measurements
token change-me
device backyard-pool
show
```

Nothing secret is written in the code. Settings live in flash (`Preferences`) and survive power loss;
`reset` erases them.

### Calibration commands (stored on the EZO boards)

```
cal ph 7        # always first: it clears the other points
cal ph 4
cal ph 10
cal orp 225     # use the value printed on your ORP solution
cal status
```

Raw EZO commands also work: `ph Slope,?`, `orp Cal,?`.

### Display

Default is SSD1306 (most 0.96″ OLEDs). For SH1106 (most 1.3″) or SSD1309 (most 2.42″),
uncomment the matching line in [`AquaSense/config.h`](AquaSense/config.h).

### HTTPS

`http://` works on your home network. With an `https://` server address the connection is encrypted, but
the certificate is not checked (`setInsecure()`). For a public server, pin its CA with
`tlsClient.setCACert()` in `postMeasurement()`.
