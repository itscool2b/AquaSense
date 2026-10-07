"""Alert rules. Pure functions, no database, so they are easy to test.

A limit alert fires when a value has stayed past its limit for `alert_minutes`
(15 by default), so one noisy reading never pages anyone. Example:

    pH > 7.8 for more than 15 minutes  ->  "pH has stayed above 7.8 for 18 minutes. Current pH: 7.93"
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import datetime, timedelta
from typing import Any, Mapping, Sequence

# (field, display name, unit, decimals)
SENSORS = (
    ("ph", "pH", "", 2),
    ("orp", "ORP", " mV", 0),
    ("temperature", "Temperature", " °C", 1),
)


@dataclass(frozen=True)
class Limits:
    """Defaults suit a chlorinated swimming pool. Change them in selfhost/.env."""

    ph_min: float | None = 7.2
    ph_max: float | None = 7.8
    orp_min: float | None = 650.0
    orp_max: float | None = None
    temperature_min: float | None = None
    temperature_max: float | None = 32.0
    alert_minutes: float = 15.0
    stale_minutes: float = 5.0

    def bounds(self, field: str) -> tuple[float | None, float | None]:
        return getattr(self, f"{field}_min"), getattr(self, f"{field}_max")

    def as_dict(self) -> dict[str, Any]:
        return asdict(self)


def _parse_optional(value: str | None, default: float | None) -> float | None:
    if value is None:
        return default
    value = value.strip()
    if value == "" or value.lower() == "none":
        return None
    return float(value)


def limits_from_env(env: Mapping[str, str]) -> Limits:
    d = Limits()
    return Limits(
        ph_min=_parse_optional(env.get("PH_MIN"), d.ph_min),
        ph_max=_parse_optional(env.get("PH_MAX"), d.ph_max),
        orp_min=_parse_optional(env.get("ORP_MIN"), d.orp_min),
        orp_max=_parse_optional(env.get("ORP_MAX"), d.orp_max),
        temperature_min=_parse_optional(env.get("TEMP_MIN"), d.temperature_min),
        temperature_max=_parse_optional(env.get("TEMP_MAX"), d.temperature_max),
        alert_minutes=float(env.get("ALERT_MINUTES") or d.alert_minutes),
        stale_minutes=float(env.get("STALE_MINUTES") or d.stale_minutes),
    )


def status(value: float | None, low: float | None, high: float | None) -> str:
    """'normal', 'low', 'high' or 'missing' for a dashboard tile."""
    if value is None:
        return "missing"
    if low is not None and value < low:
        return "low"
    if high is not None and value > high:
        return "high"
    return "normal"


def describe_minutes(minutes: float) -> str:
    minutes = int(minutes)
    if minutes < 90:
        return f"{minutes} minute{'s' if minutes != 1 else ''}"
    hours = minutes // 60
    if hours < 48:
        return f"{hours} hours"
    return f"{hours // 24} days"


def _fmt(value: float, unit: str, decimals: int) -> str:
    return f"{value:.{decimals}f}{unit}"


def _run_start(rows: Sequence[Mapping[str, Any]], field: str, outside) -> int | None:
    """Index of the first row of the unbroken run of out-of-limit readings
    that ends at the newest row, or None if the newest row is within limits."""
    start = None
    for i in range(len(rows) - 1, -1, -1):
        v = rows[i].get(field)
        if v is None or not outside(v):
            break
        start = i
    return start


def evaluate(
    rows: Sequence[Mapping[str, Any]], now: datetime, limits: Limits
) -> list[dict[str, Any]]:
    """Active alerts for one monitor.

    `rows` are that monitor's recent measurements, oldest first, each with a
    timezone-aware `timestamp` and `ph` / `orp` / `temperature` (None allowed).
    Pass at least `alert_minutes` of history; more is better for durations.
    """
    if not rows:
        return []
    latest = rows[-1]
    age = now - latest["timestamp"]
    if age > timedelta(minutes=limits.stale_minutes):
        return [
            {
                "id": "offline",
                "severity": "critical",
                "title": "Monitor offline",
                "message": f"No data for {describe_minutes(age.total_seconds() / 60)}. "
                "Check power and Wi-Fi.",
                "since": latest["timestamp"].isoformat(),
            }
        ]

    alerts: list[dict[str, Any]] = []
    window = timedelta(minutes=limits.alert_minutes)
    for field, name, unit, decimals in SENSORS:
        current = latest.get(field)
        if current is None:
            alerts.append(
                {
                    "id": f"{field}_missing",
                    "severity": "warning",
                    "title": f"{name} sensor not responding",
                    "message": f"The last upload had no {name} reading. Check the probe and its board.",
                    "since": latest["timestamp"].isoformat(),
                }
            )
            continue

        low, high = limits.bounds(field)
        checks = []
        if high is not None:
            checks.append(("high", "above", high, lambda v, h=high: v > h))
        if low is not None:
            checks.append(("low", "below", low, lambda v, lo=low: v < lo))
        for kind, word, limit, outside in checks:
            start = _run_start(rows, field, outside)
            if start is None:
                continue
            lasted = latest["timestamp"] - rows[start]["timestamp"]
            if lasted < window:
                continue
            prefix = "at least " if start == 0 else ""
            alerts.append(
                {
                    "id": f"{field}_{kind}",
                    "severity": "warning",
                    "title": f"{name} too {kind}",
                    "message": f"{name} has stayed {word} {_fmt(limit, unit, decimals)} for "
                    f"{prefix}{describe_minutes(lasted.total_seconds() / 60)}. "
                    f"Current {name}: {_fmt(current, unit, decimals)}",
                    "since": rows[start]["timestamp"].isoformat(),
                }
            )
    return alerts
