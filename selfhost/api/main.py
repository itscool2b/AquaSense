"""AquaSense ingest — you run this. We host nothing."""

from __future__ import annotations

import os
from datetime import datetime, timezone
from typing import Any

import psycopg
from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

DATABASE_URL = os.environ.get(
    "DATABASE_URL",
    "postgresql://aquasense:aquasense@127.0.0.1:5432/aquasense",
)
INGEST_TOKEN = os.environ.get("INGEST_TOKEN", "change-me")
WEB_DIR = os.environ.get("WEB_DIR", os.path.join(os.path.dirname(__file__), "..", "web"))

FIELDS = (
    "device_id",
    "ts",
    "lat",
    "lon",
    "temp_c",
    "ph",
    "spcond_ms_cm",
    "sal_psu",
    "do_mgl",
    "do_pct",
    "depth_m",
    "batt_v",
    "rssi",
    "fw",
)

app = FastAPI(title="AquaSense ingest", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


def connect():
    return psycopg.connect(DATABASE_URL, connect_timeout=5)


def check_token(body: dict[str, Any], authorization: str | None) -> None:
    token = body.get("token")
    if authorization and authorization.lower().startswith("bearer "):
        token = authorization.split(" ", 1)[1] or token
    if not token or token != INGEST_TOKEN:
        raise HTTPException(status_code=401, detail="bad token")


@app.get("/api/v1/health")
def health() -> dict[str, bool | str]:
    try:
        with connect() as conn:
            conn.execute("SELECT 1")
    except Exception as exc:
        raise HTTPException(status_code=503, detail=f"database: {exc}") from exc
    return {"ok": True, "service": "aquasense-ingest"}


@app.post("/api/v1/ingest")
async def ingest(request: Request, authorization: str | None = Header(default=None)):
    body = await request.json()
    check_token(body, authorization)
    device_id = body.get("device_id")
    if not device_id:
        raise HTTPException(status_code=400, detail="device_id required")

    ts = body.get("ts")
    if isinstance(ts, (int, float)):
        ts_dt = datetime.fromtimestamp(float(ts), tz=timezone.utc)
    elif isinstance(ts, str) and ts:
        ts_dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
    else:
        ts_dt = datetime.now(timezone.utc)

    cols = [
        "device_id",
        "ts",
        "lat",
        "lon",
        "temp_c",
        "ph",
        "spcond_ms_cm",
        "sal_psu",
        "do_mgl",
        "do_pct",
        "depth_m",
        "batt_v",
        "rssi",
        "fw",
    ]
    values = [
        str(device_id),
        ts_dt,
        body.get("lat"),
        body.get("lon"),
        body.get("temp_c"),
        body.get("ph"),
        body.get("spcond_ms_cm"),
        body.get("sal_psu"),
        body.get("do_mgl"),
        body.get("do_pct"),
        body.get("depth_m"),
        body.get("batt_v"),
        body.get("rssi"),
        body.get("fw"),
    ]
    placeholders = ", ".join(["%s"] * len(cols))
    sql = f"INSERT INTO readings ({', '.join(cols)}) VALUES ({placeholders})"
    with connect() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, values)
        conn.commit()
    return {"ok": True, "device_id": device_id}


@app.get("/api/v1/latest")
def latest(device_id: str | None = None):
    sql = "SELECT " + ", ".join(FIELDS) + " FROM readings"
    args: list[Any] = []
    if device_id:
        sql += " WHERE device_id = %s"
        args.append(device_id)
    sql += " ORDER BY ts DESC LIMIT 1"
    with connect() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, args)
            row = cur.fetchone()
    if not row:
        return {"reading": None}
    return {"reading": dict(zip(FIELDS, row, strict=True))}


@app.get("/api/v1/readings")
def readings(device_id: str | None = None, limit: int = 288):
    limit = max(1, min(limit, 2000))
    sql = "SELECT " + ", ".join(FIELDS) + " FROM readings"
    args: list[Any] = []
    if device_id:
        sql += " WHERE device_id = %s"
        args.append(device_id)
    sql += " ORDER BY ts DESC LIMIT %s"
    args.append(limit)
    with connect() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, args)
            rows = cur.fetchall()
    return {"readings": [dict(zip(FIELDS, r, strict=True)) for r in rows]}


@app.get("/")
def index():
    return FileResponse(os.path.join(WEB_DIR, "index.html"))


app.mount("/static", StaticFiles(directory=WEB_DIR), name="static")
