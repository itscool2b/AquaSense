const labels = [
  ["temp_c", "Temperature", "°C"],
  ["ph", "pH", ""],
  ["sal_psu", "Salinity", "PSU"],
  ["do_mgl", "DO", "mg/L"],
  ["depth_m", "Depth", "m"],
  ["batt_v", "Battery", "V"],
];

function fmt(v) {
  if (v === null || v === undefined || Number.isNaN(Number(v))) return "—";
  return Number(v).toFixed(2);
}

function drawChart(canvas, points, key, color, hypoxia) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  if (!points.length) return;
  const vals = points.map((p) => Number(p[key])).filter((n) => !Number.isNaN(n));
  if (!vals.length) return;
  const min = Math.min(...vals);
  const max = Math.max(...vals);
  const span = max - min || 1;
  if (hypoxia) {
    const y = h - 16 - ((2 - min) / span) * (h - 28);
    ctx.strokeStyle = "#c4452d";
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(8, y);
    ctx.lineTo(w - 8, y);
    ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  points.forEach((p, i) => {
    const x = 8 + (i / Math.max(points.length - 1, 1)) * (w - 16);
    const y = h - 16 - ((Number(p[key]) - min) / span) * (h - 28);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function inRange(rows, hours) {
  const cutoff = Date.now() - hours * 3600 * 1000;
  return rows.filter((r) => Date.parse(r.ts) >= cutoff).reverse();
}

async function refresh() {
  const [latestRes, allRes] = await Promise.all([
    fetch("/api/v1/latest"),
    fetch("/api/v1/readings?limit=2000"),
  ]);
  const latestData = await latestRes.json();
  const allData = await allRes.json();
  const r = latestData.reading;
  const rows = allData.readings || [];
  const status = document.getElementById("status");
  const gauges = document.getElementById("gauges");
  if (!r) {
    status.textContent =
      "Empty. Run python3 scripts/simulate-buoy.py (or COUNT=96 for a day of points).";
    gauges.innerHTML = "";
    document.getElementById("charts").innerHTML = "";
    document.getElementById("mapwrap").innerHTML = "";
    return;
  }
  status.textContent = `Last-seen ${r.ts} · ${r.device_id} · ${r.lat}, ${r.lon} · LTE ${r.rssi} · fw ${r.fw}`;
  gauges.innerHTML = labels
    .map(
      ([k, n, u]) =>
        `<div class="gauge"><div class="v">${fmt(r[k])}<small> ${u}</small></div><div class="n">${n}</div></div>`
    )
    .join("");

  const range = document.querySelector(".seg button.on")?.dataset.range || "24h";
  const hours = range === "7d" ? 168 : 24;
  const pts = inRange(rows, hours);
  const specs = [
    ["temp_c", "Temperature °C", "#1fa6a0", false],
    ["do_mgl", "DO mg/L", "#1fa6a0", true],
    ["sal_psu", "Salinity PSU", "#e6d5b8", false],
    ["batt_v", "Battery V", "#c4452d", false],
  ];
  const host = document.getElementById("charts");
  host.innerHTML = specs
    .map(
      ([, label], i) =>
        `<div class="chart-card"><div class="n">${label}</div><canvas id="c${i}" width="640" height="160"></canvas></div>`
    )
    .join("");
  specs.forEach(([key, , color, hyp], i) => {
    drawChart(document.getElementById("c" + i), pts, key, color, hyp);
  });

  const lat = Number(r.lat);
  const lon = Number(r.lon);
  const map = document.getElementById("mapwrap");
  if (lat && lon) {
    const b = [lon - 0.08, lat - 0.06, lon + 0.08, lat + 0.06];
    map.innerHTML = `<iframe class="map" title="Last GPS fix" src="https://www.openstreetmap.org/export/embed.html?bbox=${b[0]}%2C${b[1]}%2C${b[2]}%2C${b[3]}&amp;layer=mapnik&amp;marker=${lat}%2C${lon}"></iframe>`;
  } else {
    map.innerHTML = `<p class="muted">No GPS fix yet (lat/lon still 0). Charts still work.</p>`;
  }
}

document.querySelectorAll(".seg button").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".seg button").forEach((b) => b.classList.remove("on"));
    btn.classList.add("on");
    refresh();
  });
});

refresh();
setInterval(refresh, 15000);
