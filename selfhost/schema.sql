-- One row per upload from a monitor (every 60 s by default).
-- Plain Postgres: works in the bundled container or any hosted Postgres.
CREATE TABLE IF NOT EXISTS measurements (
  id          BIGSERIAL PRIMARY KEY,
  device_id   TEXT NOT NULL,
  timestamp   TIMESTAMPTZ NOT NULL DEFAULT now(),
  ph          DOUBLE PRECISION,           -- null when the pH board did not answer
  orp         DOUBLE PRECISION,           -- millivolts
  temperature DOUBLE PRECISION,           -- degrees C
  rssi        INTEGER,                    -- Wi-Fi signal, dBm
  fw          TEXT                        -- firmware version
);

CREATE INDEX IF NOT EXISTS measurements_device_time ON measurements (device_id, timestamp DESC);
