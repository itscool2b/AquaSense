// AquaSense dashboard: latest values, alerts, and history charts.
// Talks only to this server's /api/v1 endpoints.

const SENSORS = [
  { key: "ph", name: "pH", unit: "", decimals: 2 },
  { key: "orp", name: "ORP", unit: "mV", decimals: 0 },
  { key: "temperature", name: "Temperature", unit: "°C", decimals: 1 },
];
const STATES = {
  normal: ["✓", "var(--good)", "Normal"],
  high: ["▲", "var(--warning)", "High"],
  low: ["▼", "var(--warning)", "Low"],
  missing: ["–", "var(--mute)", "No data"],
};
const RANGE_TEXT = { "1h": "last hour", "24h": "last 24 hours", "7d": "last 7 days", "30d": "last 30 days" };

let range = "24h";
let limits = {};

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

function fmt(v, decimals) {
  return v === null || v === undefined ? "--" : Number(v).toFixed(decimals);
}

function state(v, key) {
  if (v === null || v === undefined) return "missing";
  const lo = limits[`${key}_min`];
  const hi = limits[`${key}_max`];
  if (lo !== null && lo !== undefined && v < lo) return "low";
  if (hi !== null && hi !== undefined && v > hi) return "high";
  return "normal";
}

function targetText(key, unit) {
  const lo = limits[`${key}_min`];
  const hi = limits[`${key}_max`];
  if (lo != null && hi != null) return `Target ${lo}–${hi} ${unit}`;
  if (lo != null) return `Target above ${lo} ${unit}`;
  if (hi != null) return `Target below ${hi} ${unit}`;
  return "No limit set";
}

function ago(iso) {
  const min = Math.round((Date.now() - Date.parse(iso)) / 60000);
  if (min < 1) return "just now";
  if (min < 60) return `${min} min ago`;
  if (min < 48 * 60) return `${Math.round(min / 60)} h ago`;
  return `${Math.round(min / 1440)} days ago`;
}

function timeLabel(t) {
  const d = new Date(t);
  if (range === "1h" || range === "24h") return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  return d.toLocaleDateString([], { month: "short", day: "numeric" });
}

function renderTiles(m) {
  const host = document.getElementById("tiles");
  host.replaceChildren();
  for (const s of SENSORS) {
    const v = m ? m[s.key] : null;
    const [icon, color, label] = STATES[state(v, s.key)];
    const tile = el("div", "tile");
    tile.append(el("div", "label", s.name));
    const value = el("div", "value", fmt(v, s.decimals));
    if (s.unit) value.append(el("small", null, ` ${s.unit}`));
    tile.append(value);
    const st = el("div", "state");
    const i = el("span", "icon", icon);
    i.style.color = color;
    st.append(i, document.createTextNode(label));
    tile.append(st, el("div", "target", targetText(s.key, s.unit)));
    host.append(tile);
  }
}

function renderAlerts(alerts) {
  const host = document.getElementById("alerts");
  host.replaceChildren();
  for (const a of alerts) {
    const box = el("div", `alert ${a.severity}`);
    box.append(el("strong", null, a.title), el("span", "muted", a.message));
    host.append(box);
  }
}

function drawChart(card, points, s) {
  const canvas = card.querySelector("canvas");
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  canvas.width = w * dpr;
  canvas.height = h * dpr;
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, w, h);

  const pts = points.filter((p) => p[s.key] !== null && p[s.key] !== undefined);
  const css = getComputedStyle(document.documentElement);
  const muted = css.getPropertyValue("--mute");
  ctx.font = "12px system-ui, sans-serif";
  if (!pts.length) {
    ctx.fillStyle = muted;
    ctx.fillText(`No ${s.name} data in the ${RANGE_TEXT[range]}`, 12, h / 2);
    card.hit = null;
    return;
  }

  const pad = { l: 44, r: 54, t: 10, b: 22 };
  const lo = limits[`${s.key}_min`];
  const hi = limits[`${s.key}_max`];
  const vals = pts.map((p) => p[s.key]);
  let min = Math.min(...vals, ...(lo != null ? [lo] : []), ...(hi != null ? [hi] : []));
  let max = Math.max(...vals, ...(lo != null ? [lo] : []), ...(hi != null ? [hi] : []));
  const spread = max - min || 1;
  min -= spread * 0.08;
  max += spread * 0.08;
  const t0 = Date.parse(pts[0].timestamp);
  const t1 = Date.parse(pts[pts.length - 1].timestamp);
  const x = (t) => pad.l + ((t - t0) / Math.max(t1 - t0, 1)) * (w - pad.l - pad.r);
  const y = (v) => pad.t + (1 - (v - min) / (max - min)) * (h - pad.t - pad.b);

  // Target band
  if (lo != null || hi != null) {
    const top = y(hi != null ? hi : max);
    const bottom = y(lo != null ? lo : min);
    ctx.fillStyle = "rgba(31, 166, 160, 0.08)";
    ctx.fillRect(pad.l, top, w - pad.l - pad.r, bottom - top);
  }

  // Gridlines and y labels: min, middle, max
  ctx.strokeStyle = css.getPropertyValue("--line");
  ctx.fillStyle = muted;
  ctx.lineWidth = 1;
  ctx.textAlign = "right";
  for (const v of [min, (min + max) / 2, max]) {
    ctx.beginPath();
    ctx.moveTo(pad.l, Math.round(y(v)) + 0.5);
    ctx.lineTo(w - pad.r, Math.round(y(v)) + 0.5);
    ctx.stroke();
    ctx.fillText(v.toFixed(s.decimals), pad.l - 6, y(v) + 4);
  }

  // Time labels
  ctx.textAlign = "center";
  for (let i = 0; i <= 3; i++) {
    const t = t0 + ((t1 - t0) * i) / 3;
    ctx.fillText(timeLabel(t), Math.min(Math.max(x(t), pad.l + 20), w - pad.r - 20), h - 5);
  }

  // Line, broken where readings are missing for a while
  const gap = ((t1 - t0) / Math.max(pts.length - 1, 1)) * 4;
  ctx.strokeStyle = css.getPropertyValue("--teal");
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  pts.forEach((p, i) => {
    const t = Date.parse(p.timestamp);
    const px = x(t);
    const py = y(p[s.key]);
    if (i === 0 || t - Date.parse(pts[i - 1].timestamp) > gap) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  });
  ctx.stroke();

  // End dot and latest value
  const last = pts[pts.length - 1];
  const lx = x(t1);
  const ly = y(last[s.key]);
  ctx.fillStyle = css.getPropertyValue("--ink");
  ctx.beginPath();
  ctx.arc(lx, ly, 6, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = css.getPropertyValue("--teal");
  ctx.beginPath();
  ctx.arc(lx, ly, 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = css.getPropertyValue("--foam");
  ctx.textAlign = "left";
  ctx.fillText(fmt(last[s.key], s.decimals), lx + 8, ly + 4);

  card.hit = { pts, x, s, pad, w };
}

function attachHover(card) {
  const tip = card.querySelector(".tip");
  const canvas = card.querySelector("canvas");
  canvas.addEventListener("pointermove", (ev) => {
    const hit = card.hit;
    if (!hit) return;
    const rect = canvas.getBoundingClientRect();
    const mx = ev.clientX - rect.left;
    let best = hit.pts[0];
    for (const p of hit.pts) {
      if (Math.abs(hit.x(Date.parse(p.timestamp)) - mx) < Math.abs(hit.x(Date.parse(best.timestamp)) - mx)) best = p;
    }
    tip.replaceChildren(
      el("strong", null, `${fmt(best[hit.s.key], hit.s.decimals)} ${hit.s.unit}`),
      el("div", "muted", new Date(best.timestamp).toLocaleString())
    );
    tip.style.display = "block";
    const px = hit.x(Date.parse(best.timestamp));
    tip.style.left = `${Math.min(px + 16, rect.width - tip.offsetWidth)}px`;
    tip.style.top = "36px";
  });
  canvas.addEventListener("pointerleave", () => (tip.style.display = "none"));
}

function renderCharts(points) {
  const host = document.getElementById("charts");
  if (!host.children.length) {
    for (const s of SENSORS) {
      const card = el("div", "chart");
      card.append(el("h2"), el("canvas"), el("div", "tip"));
      attachHover(card);
      host.append(card);
    }
  }
  SENSORS.forEach((s, i) => {
    const card = host.children[i];
    card.querySelector("h2").textContent = `${s.name}${s.unit ? ` (${s.unit})` : ""} · ${RANGE_TEXT[range]}`;
    card.querySelector("canvas").setAttribute("aria-label", `${s.name} chart, ${RANGE_TEXT[range]}`);
    drawChart(card, points, s);
  });
}

async function getJSON(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url}: ${res.status}`);
  return res.json();
}

let lastPoints = [];

async function refresh() {
  const charts = document.getElementById("charts");
  charts.classList.add("loading");
  try {
    if (!Object.keys(limits).length) limits = (await getJSON("/api/v1/config")).limits;
    const latest = (await getJSON("/api/v1/latest")).measurement;
    const status = document.getElementById("status");
    if (!latest) {
      status.textContent = "No measurements yet. Power on a monitor, or run: python3 scripts/simulate-device.py";
      renderTiles(null);
      renderAlerts([]);
      return;
    }
    const q = `device_id=${encodeURIComponent(latest.device_id)}`;
    const [series, alerts] = await Promise.all([
      getJSON(`/api/v1/measurements?${q}&range=${range}`),
      getJSON(`/api/v1/alerts?${q}`),
    ]);
    const rssi = latest.rssi != null ? ` · Wi-Fi ${latest.rssi} dBm` : "";
    status.textContent = `${latest.device_id} · last reading ${ago(latest.timestamp)}${rssi}`;
    renderAlerts(alerts.alerts);
    renderTiles(latest);
    lastPoints = series.points;
    renderCharts(lastPoints);
  } catch (err) {
    document.getElementById("status").textContent = `Could not reach the server (${err.message}).`;
  } finally {
    charts.classList.remove("loading");
  }
}

document.querySelectorAll(".controls button").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".controls button").forEach((b) => b.classList.remove("on"));
    btn.classList.add("on");
    range = btn.dataset.range;
    refresh();
  });
});
window.addEventListener("resize", () => lastPoints.length && renderCharts(lastPoints));

refresh();
setInterval(refresh, 30000);
