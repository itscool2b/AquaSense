# Contributing

AquaSense is an open-source monitor for pH, ORP and temperature. Fixes, new sensors, better docs and build
reports are all welcome.

## Before you open a pull request

- Pin numbers must match [`hardware/README.md`](hardware/README.md) and
  [`firmware/AquaSense/config.h`](firmware/AquaSense/config.h).
- Specs and prices need a manufacturer or seller link. If you can't cite it, leave it out.
- Keep the box low voltage: no mains wiring inside the enclosure.
- Tested your build against a reference meter? Share the numbers in an issue. Real-world data helps everyone.

## Checks

```bash
# Firmware: filter tests (no ESP32 needed) and an ESP32 compile
make -C firmware test
cd firmware && pio run

# Server: alert-rule tests
cd selfhost && pip install -r api/requirements.txt pytest && python -m pytest

# Server end to end
cd selfhost && docker compose up --build
python3 scripts/simulate-device.py
```
