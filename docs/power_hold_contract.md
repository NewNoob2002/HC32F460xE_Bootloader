# Power-hold contract

PB3 drives `MCU_ON_OFF_BAT_POWER` active high as a CMOS push-pull output. A schematic pull-up assists during reset, but firmware does not rely on it.

The sequence is protected-register unlock, raw RMU capture, PB3 output-latch preload high, PB3 high/output/CMOS configuration, 16-bit PSPCR `0x0003` write, DSB/ISB, then PB3 latch verification. A failed GPIO initialization or latch readback enters the safe fatal loop; Boot never intentionally writes PB3 low or requests a reset. `bsp_power_hold_assert()` is idempotent.

Normal, software-reset update, recovery, invalid-application, and fatal modes retain PB3 high. Application preparation reasserts it and does not deinitialize or float the pin. Hardware glitch performance remains pending oscilloscope validation over at least 100 software resets.

