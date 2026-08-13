#pragma once

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
 *
 * Depth: (p_mbar − p_atm_mbar) × 0.01 ≈ metres of water (1 dbar ≈ 1 m).
 *
 * DO mg/L: SEN0237 two-point millivolts × Benson–Krause saturation (see
 * aquasense_do_mgl_from_mv). Uncalibrated returns NaN.
 */

double aquasense_salinity_psu(double spcond_ms_cm_25c);
double aquasense_do_sat_mgl(double temp_c, double salinity_psu, double pressure_atm);
double aquasense_do_percent(double do_mgl, double do_sat_mgl);
double aquasense_depth_m(double pressure_mbar, double p_atm_mbar);

/*
 * SEN0237 two-point (DFRobot wiki): zero-oxygen solution millivolts and
 * air-saturated water millivolts, scaled to saturation mg/L at the sample
 * temperature (Benson & Krause). Uncalibrated (NaN endpoints, or air not
 * clearly above zero) returns NaN — never a fake 0–20 mg/L scale.
 */
double aquasense_do_mgl_from_mv(double mv, double mv_zero, double mv_air,
                                double sat_mgl);
