#!/usr/bin/env python3
"""POST fixture reading(s) to a local (or remote) AquaSense ingest.

COUNT=1 (default) posts one sample. COUNT=96 posts a simulated 24 h at 15 min.
"""

from __future__ import annotations

import json
import os
import time
import urllib.error
import urllib.request

URL = os.environ.get("INGEST_URL", "http://127.0.0.1:8080/api/v1/ingest")
TOKEN = os.environ.get("INGEST_TOKEN", "change-me")
COUNT = max(1, int(os.environ.get("COUNT", "1")))
DEVICE = os.environ.get("DEVICE_ID", "buoy-sim")
now = int(time.time())


def post(payload: dict) -> None:
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


for i in range(COUNT):
    ts = now - (COUNT - 1 - i) * 900
    wobble = ((i % 7) - 3) * 0.04
    post(
        {
            "device_id": DEVICE,
            "token": TOKEN,
            "ts": ts,
            "lat": 27.331,
            "lon": -82.546,
            "temp_c": round(28.4 + wobble, 2),
            "ph": round(7.82 + wobble * 0.2, 2),
            "spcond_ms_cm": 42.1,
            "sal_psu": round(27.4 + wobble, 2),
            "do_mgl": round(5.6 + wobble, 2),
            "do_pct": 86.0,
            "depth_m": 1.4,
            "batt_v": round(3.91 - i * 0.001, 3),
            "rssi": -89,
            "fw": "0.1.0-sim",
        }
    )
