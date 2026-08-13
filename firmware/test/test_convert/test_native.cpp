#include "convert.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_fails = 0;

static void expect_near(const char *name, double got, double want, double tol) {
  if (std::isnan(got) || std::fabs(got - want) > tol) {
    std::printf("FAIL %s: got %.6f want %.6f ±%.6f\n", name, got, want, tol);
    g_fails++;
  } else {
    std::printf("PASS %s\n", name);
  }
}

static void expect_true(const char *name, bool ok) {
  if (!ok) {
    std::printf("FAIL %s\n", name);
    g_fails++;
  } else {
    std::printf("PASS %s\n", name);
  }
}

int main() {
  /* PSS-78 atmospheric polynomial at R = 1 is exactly 35. */
  expect_near("salinity_s35_at_53.088", aquasense_salinity_psu(53.088), 35.0, 0.05);

  const double s10 = aquasense_salinity_psu(10.0);
  expect_true("salinity_10mScm_in_range", s10 > 4.0 && s10 < 8.0);

  /* USGS tables: ~9.09 mg/L at 20 °C, S=0, 1 atm. */
  expect_near("do_sat_20c_fresh", aquasense_do_sat_mgl(20.0, 0.0, 1.0), 9.09, 0.2);

  expect_near("do_percent_100", aquasense_do_percent(9.09, 9.09), 100.0, 0.5);

  expect_near("depth_10m", aquasense_depth_m(2013.25, 1013.25), 10.0, 0.05);

  expect_true("salinity_rejects_zero", std::isnan(aquasense_salinity_psu(0.0)));
  expect_true("depth_rejects_bad_atm", std::isnan(aquasense_depth_m(1013.25, 0.0)));

  /* SEN0237 two-point: zero solution → 0, air-sat → sat, midpoint → sat/2. */
  expect_near("do_mv_zero", aquasense_do_mgl_from_mv(40.0, 40.0, 1600.0, 9.09), 0.0,
              0.01);
  expect_near("do_mv_air", aquasense_do_mgl_from_mv(1600.0, 40.0, 1600.0, 9.09), 9.09,
              0.01);
  expect_near("do_mv_mid", aquasense_do_mgl_from_mv(820.0, 40.0, 1600.0, 9.09), 4.545,
              0.05);
  expect_true("do_mv_uncalibrated",
              std::isnan(aquasense_do_mgl_from_mv(800.0, NAN, NAN, 9.09)));
  expect_true("do_mv_bad_span",
              std::isnan(aquasense_do_mgl_from_mv(800.0, 1600.0, 40.0, 9.09)));

  if (g_fails) {
    std::printf("%d test(s) failed\n", g_fails);
    return 1;
  }
  std::printf("all conversion tests passed\n");
  return 0;
}
