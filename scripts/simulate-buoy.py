#!/usr/bin/env python3
"""POST one fixture reading to a local (or remote) AquaSense ingest."""

from __future__ import annotations

import json
import os
import time
import urllib.error
import urllib.request

URL = os.environ.get("INGEST_URL", "http://127.0.0.1:8080/api/v1/ingest")
TOKEN = os.environ.get("INGEST_TOKEN", "change-me")

payload = {
    "device_id": os.environ.get("DEVICE_ID", "buoy-sim"),
    "token": TOKEN,
    "ts": int(time.time()),
    "lat": 27.331,
    "lon": -82.546,
    "temp_c": 28.4,
    "ph": 7.82,
    "spcond_ms_cm": 42.1,
    "sal_psu": 27.4,
    "do_mgl": 5.6,
    "do_pct": 86.0,
    "depth_m": 1.4,
    "batt_v": 3.91,
    "rssi": -89,
    "fw": "0.1.0-sim",
}

data = json.dumps(payload).encode("utf-8")
req = urllib.request.Request(
    URL,
    data=data,
    method="POST",
    headers={
        "Content-Type": "application/json",
        "Authorization": f"Bearer {TOKEN}",
    },
)

try:
    with urllib.request.urlopen(req, timeout=10) as resp:
        body = resp.read().decode("utf-8")
        print(resp.status, body)
except urllib.error.HTTPError as err:
    print(err.status, err.read().decode("utf-8", errors="replace"))
    raise SystemExit(1)
except urllib.error.URLError as err:
    print("could not reach", URL, err)
    raise SystemExit(1)
