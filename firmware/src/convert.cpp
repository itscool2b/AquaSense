/*
 * AquaSense conversion helpers — portable C++ (no Arduino).
 *
 * Salinity: UNESCO Practical Salinity Scale 1978 (PSS-78) at p = 0 dbar,
 * using conductivity already temperature-compensated to 25 °C.
 * Reference: Fofonoff & Millard 1983, UNESCO technical papers in marine
 * science 44. 25 °C seawater S=35 specific conductance ≈ 53.088 mS/cm
 * (Culkin & Smith / UNESCO).
 *
 * Dissolved-oxygen saturation: Benson & Krause (1984) as used by USGS
 * TWRI Book 9, Chapter A6 — ln(DO*) in mg/L at 1 atm, scaled by pressure.
 */
#include "convert.h"
#include <cmath>

static constexpr double K25_S35_MSCM = 53.088;

double aquasense_salinity_psu(double spcond_ms_cm_25c) {
  if (!(spcond_ms_cm_25c > 0.0) || spcond_ms_cm_25c > 200.0) {
    return NAN;
  }
  const double r = spcond_ms_cm_25c / K25_S35_MSCM;
  const double r05 = std::sqrt(r);
  const double s =
      0.0080 - 0.1692 * r05 + 25.3851 * r + 14.0941 * r * r05 -
      7.0261 * r * r + 2.7081 * r * r * r05;
  if (s < 0.0) {
    return 0.0;
  }
  return s;
}

double aquasense_do_sat_mgl(double temp_c, double salinity_psu, double pressure_atm) {
  if (temp_c < -2.0 || temp_c > 40.0 || salinity_psu < 0.0 || salinity_psu > 42.0) {
    return NAN;
  }
  if (!(pressure_atm > 0.2) || pressure_atm > 2.0) {
    pressure_atm = 1.0;
  }
  const double t = temp_c + 273.15;
  const double ln_do =
      -139.34411 + (1.575701e5) / t - (6.642308e7) / (t * t) +
      (1.243800e10) / (t * t * t) - (8.621949e11) / (t * t * t * t) -
      salinity_psu * (0.017674 - 10.754 / t + 2140.7 / (t * t));
  return std::exp(ln_do) * pressure_atm;
}

double aquasense_do_percent(double do_mgl, double do_sat_mgl) {
  if (!(do_sat_mgl > 0.0) || !(do_mgl >= 0.0)) {
    return NAN;
  }
  return 100.0 * do_mgl / do_sat_mgl;
}

double aquasense_depth_m(double pressure_mbar, double p_atm_mbar) {
  if (!(pressure_mbar > 0.0) || !(p_atm_mbar > 0.0)) {
    return NAN;
  }
  const double dbar = (pressure_mbar - p_atm_mbar) * 0.01;
  if (dbar < 0.0) {
    return 0.0;
  }
  return dbar;
}

double aquasense_do_mgl_from_mv(double mv, double mv_zero, double mv_air,
                                double sat_mgl) {
  if (!std::isfinite(mv) || !std::isfinite(mv_zero) || !std::isfinite(mv_air) ||
      !std::isfinite(sat_mgl)) {
    return NAN;
  }
  if (!(sat_mgl > 0.0)) {
    return NAN;
  }
  /* Galvanic DO: air-sat millivolts must sit clearly above the zero point. */
  if (!(mv_air > mv_zero + 10.0)) {
    return NAN;
  }
  const double do_mgl = (mv - mv_zero) / (mv_air - mv_zero) * sat_mgl;
  if (do_mgl < 0.0) {
    return 0.0;
  }
  return do_mgl;
}
