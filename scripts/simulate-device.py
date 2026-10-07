#!/usr/bin/env python3
"""Send test measurements to an AquaSense server, as if a monitor were online.

    python3 scripts/simulate-device.py            # one reading, now
    COUNT=1440 python3 scripts/simulate-device.py # a day of readings, 1 per minute

Env: SERVER (default http://127.0.0.1:8080), INGEST_TOKEN (default change-me),
DEVICE_ID (default sim-pool). Values follow a pool's daily cycle: warmer and
lower ORP in the afternoon sun.
"""

from __future__ import annotations

import json
import math
import os
import random
import time
import urllib.error
import urllib.request

SERVER = os.environ.get("SERVER", "http://127.0.0.1:8080").rstrip("/")
TOKEN = os.environ.get("INGEST_TOKEN", "change-me")
DEVICE = os.environ.get("DEVICE_ID", "sim-pool")
COUNT = max(1, int(os.environ.get("COUNT", "1")))


def post(payload: dict) -> None:
    req = urllib.request.Request(
        f"{SERVER}/api/v1/measurements",
        data=json.dumps(payload).encode(),
        method="POST",
        headers={"Content-Type": "application/json", "Authorization": f"Bearer {TOKEN}"},
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            resp.read()
    except urllib.error.HTTPError as err:
        raise SystemExit(f"{err.code} {err.read().decode(errors='replace')}")
    except urllib.error.URLError as err:
        raise SystemExit(f"could not reach {SERVER}: {err.reason}")


rng = random.Random(7)
now = int(time.time())
for i in range(COUNT):
    ts = now - (COUNT - 1 - i) * 60
    day = (ts % 86400) / 86400  # 0..1 through the UTC day
    sun = max(0.0, math.sin((day - 0.25) * 2 * math.pi))  # 0 at night, 1 at midday
    post(
        {
            "device_id": DEVICE,
            "timestamp": ts,
            "ph": round(7.45 + 0.04 * sun + rng.gauss(0, 0.01), 2),
            "orp": round(705 - 45 * sun + rng.gauss(0, 3)),
            "temperature": round(26.5 + 1.8 * sun + rng.gauss(0, 0.05), 2),
            "rssi": -58,
            "fw": "sim",
        }
    )
print(f"sent {COUNT} measurement(s) for {DEVICE} to {SERVER}")
