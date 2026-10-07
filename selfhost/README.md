# AquaSense server and dashboard

Stores measurements from your monitors in Postgres, serves the dashboard, and checks the alert rules.
It runs on any computer with Docker: a Raspberry Pi, an old laptop, or a small cloud server.

```bash
cp .env.example .env          # set INGEST_TOKEN to a long random string
docker compose up -d --build
```

Open `http://<this-computer's-IP>:8080`. Try it without hardware:

```bash
python3 ../scripts/simulate-device.py                # one reading
COUNT=1440 python3 ../scripts/simulate-device.py     # a day of readings
```

## API

| Method | Path | What it does |
|---|---|---|
| `POST` | `/api/v1/measurements` | Store one measurement. Header `Authorization: Bearer <INGEST_TOKEN>`. |
| `GET` | `/api/v1/latest` | Newest measurement |
| `GET` | `/api/v1/measurements?range=1h\|24h\|7d\|30d` | History; longer ranges are averaged into buckets |
| `GET` | `/api/v1/alerts` | Active alerts |
| `GET` | `/api/v1/health` | Server and database check |

Each GET accepts `device_id=` when you run more than one monitor.

Measurement body (what the monitor sends; `null` means that sensor did not answer):

```json
{"device_id": "backyard-pool", "ph": 7.42, "orp": 681, "temperature": 27.4, "rssi": -61, "fw": "1.0.0"}
```

Database table ([`schema.sql`](schema.sql)): `measurements(id, device_id, timestamp, ph, orp, temperature, rssi, fw)`.

## Alerts

Limits and timing live in `.env` (see [`.env.example`](.env.example)). A limit alert fires only after the
value has been out of range for `ALERT_MINUTES` (15) in a row, so one noisy reading never triggers it.
The rules are in [`api/alerts.py`](api/alerts.py), with tests in [`tests/`](tests/):

```bash
pip install -r api/requirements.txt pytest && python -m pytest
```

Set `ALERT_WEBHOOK_URL=https://ntfy.sh/<your-secret-topic>` and subscribe to that topic in the
[ntfy](https://ntfy.sh) app to get a phone notification when an alert starts.

## Reaching it from outside your home

The monitor and the server only need to share a network. To check the dashboard away from home, run the
stack on a cloud server, or put it behind a tunnel, with an `https://` address. Then give the monitor
that address with `server https://.../api/v1/measurements`.
