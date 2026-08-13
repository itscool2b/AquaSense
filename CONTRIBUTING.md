# Contributing

AquaSense is a solar buoy that sits on the water, measures a few stats, and
uses cellular internet to send them to a website you host.

## Before you open a PR

- Pin numbers must match [hardware/pinmap.md](hardware/pinmap.md).
- Specs must cite a manufacturer URL. If you cannot cite it, omit it.
- Buy links must open the correct SKU.
- Do not add a hosted cloud, a second kit SKU, or turbidity as NTU.

## Firmware

```bash
cd firmware
pio test -e native
pio run -e lilygo-t-a7670g
```

## Site

```bash
cd site
npm install
npm run build
```

## Self-host

```bash
cd selfhost
docker compose up --build
python3 ../scripts/simulate-buoy.py
COUNT=96 python3 ../scripts/simulate-buoy.py   # 24h of chart points
```
