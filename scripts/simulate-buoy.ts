/**
 * POST one fixture reading to a local (or remote) AquaSense ingest.
 *   INGEST_URL=http://127.0.0.1:8080/api/v1/ingest npx --yes tsx scripts/simulate-buoy.ts
 */
const url = process.env.INGEST_URL ?? "http://127.0.0.1:8080/api/v1/ingest";
const token = process.env.INGEST_TOKEN ?? "change-me";

const payload = {
  device_id: process.env.DEVICE_ID ?? "buoy-sim",
  token,
  ts: Math.floor(Date.now() / 1000),
  lat: 27.331,
  lon: -82.546,
  temp_c: 28.4,
  ph: 7.82,
  spcond_ms_cm: 42.1,
  sal_psu: 27.4,
  do_mgl: 5.6,
  do_pct: 86.0,
  depth_m: 1.4,
  batt_v: 3.91,
  rssi: -89,
  fw: "0.1.0-sim",
};

const res = await fetch(url, {
  method: "POST",
  headers: {
    "Content-Type": "application/json",
    Authorization: `Bearer ${token}`,
  },
  body: JSON.stringify(payload),
});

const text = await res.text();
console.log(res.status, text);
if (!res.ok) process.exit(1);
