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

async function refresh() {
  const res = await fetch("/api/v1/latest");
  const data = await res.json();
  const r = data.reading;
  const status = document.getElementById("status");
  const gauges = document.getElementById("gauges");
  if (!r) {
    status.textContent =
      "Empty. Run python3 scripts/simulate-buoy.py or wait for the buoy to POST.";
    gauges.innerHTML = "";
    return;
  }
  status.textContent = `Last-seen ${r.ts} · ${r.device_id} · ${r.lat}, ${r.lon} · LTE ${r.rssi} · fw ${r.fw}`;
  gauges.innerHTML = labels
    .map(
      ([k, n, u]) =>
        `<div class="gauge"><div class="v">${fmt(r[k])}<small> ${u}</small></div><div class="n">${n}</div></div>`
    )
    .join("");
}

refresh();
setInterval(refresh, 15000);
