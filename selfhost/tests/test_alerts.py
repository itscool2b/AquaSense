"""Run: cd selfhost && python -m pytest"""

from datetime import datetime, timedelta, timezone

from api.alerts import Limits, describe_minutes, evaluate, limits_from_env, status

NOW = datetime(2026, 7, 4, 14, 0, tzinfo=timezone.utc)
LIMITS = Limits()


def rows(values, field="ph", every_min=1, end=NOW, base=None):
    """One row per minute ending at `end`; other fields stay normal."""
    base = base or {"ph": 7.5, "orp": 700.0, "temperature": 27.0}
    out = []
    for i, v in enumerate(values):
        ts = end - timedelta(minutes=every_min * (len(values) - 1 - i))
        out.append({**base, field: v, "timestamp": ts})
    return out


def ids(alerts):
    return [a["id"] for a in alerts]


def test_all_normal_no_alerts():
    assert evaluate(rows([7.5] * 30), NOW, LIMITS) == []


def test_ph_high_for_more_than_15_minutes():
    data = rows([7.5] * 10 + [7.93] * 18)
    alerts = evaluate(data, NOW, LIMITS)
    assert ids(alerts) == ["ph_high"]
    assert alerts[0]["message"] == (
        "pH has stayed above 7.80 for 17 minutes. Current pH: 7.93"
    )


def test_ph_high_for_less_than_15_minutes_is_quiet():
    data = rows([7.5] * 10 + [7.93] * 10)
    assert evaluate(data, NOW, LIMITS) == []


def test_one_good_reading_resets_the_clock():
    data = rows([7.9] * 10 + [7.7] + [7.9] * 10)
    assert evaluate(data, NOW, LIMITS) == []


def test_ph_low_and_orp_low():
    base = {"ph": 7.0, "orp": 600.0, "temperature": 27.0}
    data = rows([7.0] * 20, base=base)
    assert ids(evaluate(data, NOW, LIMITS)) == ["ph_low", "orp_low"]


def test_temperature_high():
    data = rows([33.5] * 20, field="temperature")
    alerts = evaluate(data, NOW, LIMITS)
    assert ids(alerts) == ["temperature_high"]
    assert "Current Temperature: 33.5 °C" in alerts[0]["message"]


def test_run_covering_all_history_says_at_least():
    data = rows([7.9] * 20)
    assert "at least 19 minutes" in evaluate(data, NOW, LIMITS)[0]["message"]


def test_missing_sensor():
    data = rows([7.5] * 5)
    data[-1]["orp"] = None
    assert ids(evaluate(data, NOW, LIMITS)) == ["orp_missing"]


def test_offline_replaces_other_alerts():
    data = rows([7.95] * 30, end=NOW - timedelta(minutes=12))
    alerts = evaluate(data, NOW, LIMITS)
    assert ids(alerts) == ["offline"]
    assert "12 minutes" in alerts[0]["message"]


def test_no_rows_no_alerts():
    assert evaluate([], NOW, LIMITS) == []


def test_status():
    assert status(7.5, 7.2, 7.8) == "normal"
    assert status(7.9, 7.2, 7.8) == "high"
    assert status(7.0, 7.2, 7.8) == "low"
    assert status(None, 7.2, 7.8) == "missing"
    assert status(40.0, None, None) == "normal"


def test_limits_from_env():
    limits = limits_from_env({"PH_MAX": "8.2", "ORP_MIN": "", "TEMP_MAX": "none"})
    assert limits.ph_max == 8.2
    assert limits.ph_min == 7.2
    assert limits.orp_min is None
    assert limits.temperature_max is None


def test_disabled_limit_never_alerts():
    limits = limits_from_env({"PH_MAX": ""})
    assert evaluate(rows([9.0] * 30), NOW, limits) == []


def test_describe_minutes():
    assert describe_minutes(1) == "1 minute"
    assert describe_minutes(45) == "45 minutes"
    assert describe_minutes(150) == "2 hours"
    assert describe_minutes(60 * 72) == "3 days"
