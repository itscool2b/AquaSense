"""AquaSense server: stores measurements, serves the dashboard, raises alerts.

You run this (docker compose up). There is no AquaSense cloud.
"""

from __future__ import annotations

import logging
import os
import re
import threading
import urllib.request
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone
from typing import Any

import psycopg
from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from psycopg.rows import dict_row

from .alerts import evaluate, limits_from_env

DATABASE_URL = os.environ.get(
    "DATABASE_URL", "postgresql://aquasense:aquasense@127.0.0.1:5432/aquasense"
)
INGEST_TOKEN = os.environ.get("INGEST_TOKEN", "change-me")
WEB_DIR = os.environ.get("WEB_DIR", os.path.join(os.path.dirname(__file__), "..", "web"))
ALERT_WEBHOOK_URL = os.environ.get("ALERT_WEBHOOK_URL", "").strip()
LIMITS = limits_from_env(os.environ)

DEVICE_ID = re.compile(r"^[A-Za-z0-9._-]{1,32}$")
SENSOR_FIELDS = ("ph", "orp", "temperature")

# Longer ranges are averaged into buckets so a 30-day chart is ~360 points.
RANGES: dict[str, tuple[timedelta, str | None]] = {
    "1h": (timedelta(hours=1), None),
    "24h": (timedelta(hours=24), "5 minutes"),
    "7d": (timedelta(days=7), "30 minutes"),
    "30d": (timedelta(days=30), "2 hours"),
}

log = logging.getLogger("aquasense")


def connect():
    return psycopg.connect(DATABASE_URL, connect_timeout=5, row_factory=dict_row)


def iso(row: dict[str, Any] | None) -> dict[str, Any] | None:
    if row and isinstance(row.get("timestamp"), datetime):
        row = {**row, "timestamp": row["timestamp"].isoformat()}
    return row


def check_token(body: dict[str, Any], authorization: str | None) -> None:
    token = body.get("token")
    if authorization and authorization.lower().startswith("bearer "):
        token = authorization.split(" ", 1)[1].strip() or token
    if not token or token != INGEST_TOKEN:
        raise HTTPException(status_code=401, detail="bad token")


def number_or_none(body: dict[str, Any], key: str) -> float | None:
    value = body.get(key)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise HTTPException(status_code=422, detail=f"{key} must be a number or null")
    return float(value)


def parse_timestamp(value: Any) -> datetime:
    if value is None:
        return datetime.now(timezone.utc)
    try:
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            return datetime.fromtimestamp(float(value), tz=timezone.utc)
        if isinstance(value, str):
            ts = datetime.fromisoformat(value.replace("Z", "+00:00"))
            return ts if ts.tzinfo else ts.replace(tzinfo=timezone.utc)
    except (ValueError, OverflowError, OSError):
        pass
    raise HTTPException(status_code=422, detail="timestamp must be ISO 8601 or Unix seconds")


def resolve_device(conn, device_id: str | None) -> str | None:
    """The requested monitor, or the one that reported most recently."""
    if device_id:
        return device_id
    row = conn.execute(
        "SELECT device_id FROM measurements ORDER BY timestamp DESC LIMIT 1"
    ).fetchone()
    return row["device_id"] if row else None


def alerts_for(conn, device_id: str, now: datetime) -> list[dict[str, Any]]:
    rows = conn.execute(
        "SELECT timestamp, ph, orp, temperature FROM measurements "
        "WHERE device_id = %s AND timestamp >= %s ORDER BY timestamp",
        (device_id, now - timedelta(hours=24)),
    ).fetchall()
    if not rows:
        latest = conn.execute(
            "SELECT timestamp, ph, orp, temperature FROM measurements "
            "WHERE device_id = %s ORDER BY timestamp DESC LIMIT 1",
            (device_id,),
        ).fetchone()
        rows = [latest] if latest else []
    return evaluate(rows, now, LIMITS)


# ---------------------------------------------------------------- notifier --


def send_notification(device_id: str, alert: dict[str, Any]) -> None:
    """Plain-text POST. Works as-is with ntfy.sh (free phone push notifications)."""
    req = urllib.request.Request(
        ALERT_WEBHOOK_URL,
        data=f"{device_id}: {alert['message']}".encode("utf-8"),
        method="POST",
        headers={
            "Content-Type": "text/plain; charset=utf-8",
            "Title": f"AquaSense: {alert['title']}",
            "Tags": "warning",
        },
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        resp.read()


def notifier_loop(stop: threading.Event) -> None:
    """Every minute, notify once for each alert that just started."""
    active: set[tuple[str, str]] = set()
    while not stop.wait(60):
        try:
            now = datetime.now(timezone.utc)
            current: set[tuple[str, str]] = set()
            with connect() as conn:
                devices = conn.execute(
                    "SELECT DISTINCT device_id FROM measurements WHERE timestamp >= %s",
                    (now - timedelta(days=1),),
                ).fetchall()
                for d in devices:
                    for alert in alerts_for(conn, d["device_id"], now):
                        key = (d["device_id"], alert["id"])
                        current.add(key)
                        if key not in active:
                            send_notification(d["device_id"], alert)
            active = current
        except Exception:  # keep checking through database or network hiccups
            log.exception("alert check failed")


@asynccontextmanager
async def lifespan(_app: FastAPI):
    stop = threading.Event()
    if ALERT_WEBHOOK_URL:
        threading.Thread(target=notifier_loop, args=(stop,), daemon=True).start()
    yield
    stop.set()


app = FastAPI(title="AquaSense", version="1.0.0", lifespan=lifespan)


# --------------------------------------------------------------------- API --


@app.get("/api/v1/health")
def health() -> dict[str, Any]:
    try:
        with connect() as conn:
            conn.execute("SELECT 1")
    except Exception as exc:
        raise HTTPException(status_code=503, detail=f"database: {exc}") from exc
    return {"ok": True, "service": "aquasense"}


@app.post("/api/v1/measurements")
async def add_measurement(request: Request, authorization: str | None = Header(default=None)):
    try:
        body = await request.json()
    except ValueError as exc:
        raise HTTPException(status_code=400, detail="body must be JSON") from exc
    if not isinstance(body, dict):
        raise HTTPException(status_code=400, detail="body must be a JSON object")
    check_token(body, authorization)

    device_id = body.get("device_id")
    if not isinstance(device_id, str) or not DEVICE_ID.match(device_id):
        raise HTTPException(status_code=422, detail="device_id: 1-32 letters, digits, . _ -")
    values = [number_or_none(body, f) for f in SENSOR_FIELDS]
    rssi = body.get("rssi")
    rssi = int(rssi) if isinstance(rssi, (int, float)) and not isinstance(rssi, bool) else None
    fw = body.get("fw")
    fw = fw[:32] if isinstance(fw, str) else None
    ts = parse_timestamp(body.get("timestamp"))

    with connect() as conn:
        conn.execute(
            "INSERT INTO measurements (device_id, timestamp, ph, orp, temperature, rssi, fw) "
            "VALUES (%s, %s, %s, %s, %s, %s, %s)",
            (device_id, ts, *values, rssi, fw),
        )
    return {"ok": True, "device_id": device_id}


@app.get("/api/v1/latest")
def latest(device_id: str | None = None):
    with connect() as conn:
        device = resolve_device(conn, device_id)
        row = None
        if device:
            row = conn.execute(
                "SELECT device_id, timestamp, ph, orp, temperature, rssi, fw FROM measurements "
                "WHERE device_id = %s ORDER BY timestamp DESC LIMIT 1",
                (device,),
            ).fetchone()
    return {"measurement": iso(row)}


@app.get("/api/v1/measurements")
def measurements(device_id: str | None = None, range: str = "24h"):  # noqa: A002
    if range not in RANGES:
        raise HTTPException(status_code=422, detail=f"range must be one of {', '.join(RANGES)}")
    span, bucket = RANGES[range]
    since = datetime.now(timezone.utc) - span
    with connect() as conn:
        device = resolve_device(conn, device_id)
        if not device:
            return {"device_id": None, "range": range, "points": []}
        if bucket is None:
            rows = conn.execute(
                "SELECT timestamp, ph, orp, temperature FROM measurements "
                "WHERE device_id = %s AND timestamp >= %s ORDER BY timestamp",
                (device, since),
            ).fetchall()
        else:
            rows = conn.execute(
                "SELECT date_bin(%s::interval, timestamp, TIMESTAMPTZ '2000-01-01') AS timestamp, "
                "avg(ph) AS ph, avg(orp) AS orp, avg(temperature) AS temperature "
                "FROM measurements WHERE device_id = %s AND timestamp >= %s "
                "GROUP BY 1 ORDER BY 1",
                (bucket, device, since),
            ).fetchall()
    return {"device_id": device, "range": range, "points": [iso(r) for r in rows]}


@app.get("/api/v1/alerts")
def alerts(device_id: str | None = None):
    with connect() as conn:
        device = resolve_device(conn, device_id)
        found = alerts_for(conn, device, datetime.now(timezone.utc)) if device else []
    return {"device_id": device, "alerts": found}


@app.get("/api/v1/config")
def config():
    return {"limits": LIMITS.as_dict()}


@app.get("/")
def index():
    return FileResponse(os.path.join(WEB_DIR, "index.html"))


app.mount("/static", StaticFiles(directory=WEB_DIR), name="static")
