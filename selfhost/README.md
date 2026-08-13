# Self-host AquaSense

A solar buoy that sits on the water, measures a few stats, uses cellular
internet to send them to a website **you** host.

```bash
cp .env.example .env
docker compose up --build
```

Open http://127.0.0.1:8080 then:

```bash
python3 ../scripts/simulate-buoy.py
COUNT=96 python3 ../scripts/simulate-buoy.py
```

`POST /api/v1/ingest` expects JSON with `device_id`, `token` (or `Authorization: Bearer`),
and the sensor fields. Token must match `INGEST_TOKEN`.

The LTE buoy cannot reach localhost. Put this stack on a VPS (or a tunnel) and
use that `https://` URL in the SD `config.json`.
