#include <unity.h>
#include "convert.h"
#include <cmath>

void test_salinity_s35_at_k25() {
  TEST_ASSERT_FLOAT_WITHIN(0.4, 35.0, aquasense_salinity_psu(53.088));
}

void test_salinity_low() {
  const double s = aquasense_salinity_psu(10.0);
  TEST_ASSERT_TRUE(s > 4.0 && s < 8.0);
}

void test_do_sat_20c_fresh() {
  /* USGS tables: ~9.09 mg/L at 20 °C, S=0, 1 atm. */
  TEST_ASSERT_FLOAT_WITHIN(0.2, 9.09, aquasense_do_sat_mgl(20.0, 0.0, 1.0));
}

void test_do_percent() {
  TEST_ASSERT_FLOAT_WITHIN(0.5, 100.0, aquasense_do_percent(9.09, 9.09));
}

void test_depth_10m() {
  TEST_ASSERT_FLOAT_WITHIN(0.05, 10.0, aquasense_depth_m(2013.25, 1013.25));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_salinity_s35_at_k25);
  RUN_TEST(test_salinity_low);
  RUN_TEST(test_do_sat_20c_fresh);
  RUN_TEST(test_do_percent);
  RUN_TEST(test_depth_10m);
  return UNITY_END();
}
