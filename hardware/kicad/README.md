# Optional KiCad carrier

The default AquaSense build is **Gravity cables on a screw-terminal breakout**. You do not need this folder to finish the nine steps.

`aquasense.kicad_sch` is the same GPIO netlist as [../pinmap.md](../pinmap.md), `firmware/include/pins.h`, and [../diagrams/wiring.svg](../diagrams/wiring.svg):

- pH analog GPIO **32**, EC **34**, DO **39**, DS18B20 **18**, MS5803 SDA/SCL **16 / 17**
- GPIO **4** (PWRKEY) and **5** (RST) are no-connect
- GPS SKU: GPIO **21 / 22** are GNSS UART, not I2C

Open with KiCad 8 or 9. There is no PCB layout — this is a netlist schematic for anyone who later wants a carrier. If a footprint disagrees with the pin table, the pin table wins.
