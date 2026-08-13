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
