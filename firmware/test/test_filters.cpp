// Computer-side tests for AquaSense/filters.cpp. Run: make -C firmware test
#include "filters.h"

#include <cmath>
#include <cstdio>

static int g_fails = 0;

static void expect_near(const char *name, double got, double want, double tol) {
  if (std::isnan(got) || std::fabs(got - want) > tol) {
    std::printf("FAIL %s: got %.4f want %.4f\n", name, got, want);
    g_fails++;
  } else {
    std::printf("PASS %s\n", name);
  }
}

static void expect_true(const char *name, bool ok) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) g_fails++;
}

static const SensorLimits kPh = {0.0f, 14.0f, 0.5f};
static const SensorLimits kTemp = {-10.0f, 80.0f, 2.0f};

int main() {
  {
    SmoothedReading ph(kPh);
    expect_true("empty has no value", !ph.has_value() && std::isnan(ph.value()));
    ph.add(7.40f);
    ph.add(7.42f);
    ph.add(7.44f);
    expect_near("mean of three", ph.value(), 7.42, 0.001);
  }

  {
    // The 12.98 spike from the build guide is dropped; the average stays put.
    SmoothedReading ph(kPh);
    const float seq[] = {7.41f, 7.40f, 7.42f, 12.98f, 7.41f};
    bool accepted[5];
    for (int i = 0; i < 5; i++) accepted[i] = ph.add(seq[i]);
    expect_true("spike rejected", !accepted[3]);
    expect_true("reading after spike accepted", accepted[4]);
    expect_near("spike does not move average", ph.value(), 7.41, 0.01);
    expect_true("one rejection counted", ph.rejected() == 1);
  }

  {
    SmoothedReading ph(kPh);
    expect_true("pH below 0 rejected", !ph.add(-0.5f));
    expect_true("pH above 14 rejected", !ph.add(14.2f));
    expect_true("NaN rejected", !ph.add(NAN));
    expect_true("still empty", !ph.has_value());
  }

  {
    // DS18B20 reports -127 C when unplugged: outside the range, never averaged.
    SmoothedReading t(kTemp);
    t.add(25.6f);
    expect_true("disconnected probe rejected", !t.add(-127.0f));
    expect_near("temperature unchanged", t.value(), 25.6, 0.001);
  }

  {
    // Probe moved from pH 7 buffer to pH 4 buffer: a real change, not noise.
    SmoothedReading ph(kPh);
    for (int i = 0; i < 10; i++) ph.add(7.00f);
    ph.add(4.02f);
    ph.add(4.01f);
    bool third = ph.add(4.00f);
    expect_true("three agreeing jumps accepted", third);
    expect_near("window moved to new level", ph.value(), 4.01, 0.01);
    expect_true("held readings not counted as rejected", ph.rejected() == 0);
  }

  {
    // Scattered jumps are noise and never become the new level.
    SmoothedReading ph(kPh);
    for (int i = 0; i < 10; i++) ph.add(7.00f);
    ph.add(9.0f);
    ph.add(11.0f);
    ph.add(5.0f);
    expect_near("scattered jumps ignored", ph.value(), 7.00, 0.001);
  }

  {
    // Window keeps the last 10 readings.
    SmoothedReading t(kTemp);
    for (int i = 0; i < 10; i++) t.add(20.0f);
    for (int i = 0; i < 10; i++) t.add(21.0f);
    expect_near("old readings roll out", t.value(), 21.0, 0.001);
    expect_true("window capped at 10", t.count() == SmoothedReading::kWindow);
  }

  {
    // A board that stops answering clears after 5 misses in a row.
    SmoothedReading ph(kPh);
    ph.add(7.2f);
    for (int i = 0; i < 4; i++) ph.miss();
    expect_true("value kept after 4 misses", ph.has_value());
    ph.miss();
    expect_true("value dropped after 5 misses", !ph.has_value());
    ph.add(7.3f);
    expect_near("recovers on next reading", ph.value(), 7.3, 0.001);
  }

  if (g_fails) {
    std::printf("%d test(s) failed\n", g_fails);
    return 1;
  }
  std::printf("all filter tests passed\n");
  return 0;
}
