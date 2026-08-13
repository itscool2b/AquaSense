// Electronics tray for a Pelican 1120-class box. Units: mm.
// Print in PETG. Not required — foam + zip ties also work.

tray_l = 168;
tray_w = 110;
tray_h = 8;
wall = 2;

difference() {
  cube([tray_l, tray_w, tray_h]);
  translate([wall, wall, 2])
    cube([tray_l - 2 * wall, tray_w - 2 * wall, tray_h]);
}

// Board pocket (T-A7670G ~111 x 34 mm)
translate([8, 8, 2])
  difference() {
    cube([115, 38, 4]);
    translate([1, 1, 1]) cube([113, 36, 4]);
  }
