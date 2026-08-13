# AquaSense firmware

A solar buoy that sits on the water, measures a few stats, uses cellular
internet to send them to a website you host.

Board: **LilyGO T-A7670G R2 with GPS**. HTTPS via TinyGSM-fork (lewisxhe).
ADC is ESP32 12-bit `analogReadMilliVolts()`.

```bash
# conversion tests (no hardware)
make test

# flash
pio run -e lilygo-t-a7670g -t upload
```

Copy `data/config.example.json` to the microSD card as `config.json` and set
**your** `ingest_url` and `token`. Pin map: [`../hardware/pinmap.md`](../hardware/pinmap.md).

After boot, serial waits 12 seconds for `cal` (or set `"stay_awake": true`).
Dissolved oxygen is a two-point in NVS (`doair` / `dozero`). Uncalibrated
POSTs send `do_mgl: null`, not a fake 0–20 mg/L scale. pH/EC use the DFRobot
serial commands (`enterph` / `enterec`). Battery volts: LilyGO V1.2+
`analogReadMilliVolts() * 2` on GPIO 35.

