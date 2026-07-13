# Power-hold contract

PB3 controls the Linux main-board power and is active high. The bootloader applies a conservative all-reset policy because the reference contains no narrower historical policy: PB3 is asserted for power-on, pin, software, watchdog, brownout/PVD, clock-failure, and multi-source reset flags.

Initialization is protection unlock, raw RMU capture, PB3 output-latch preload high with `GPIO_SetPins`, then `GPIO_Init` with `PIN_STAT_SET`, `PIN_DIR_OUT`, and `PIN_OUT_TYPE_CMOS`, followed by latch readback. No Boot path writes PB3 low. Recovery, update, fatal-safe loop, and application handover reassert or retain high. GPIO is not deinitialized during jump.

Hardware validation is pending. On an oscilloscope, PB3 must remain above the downstream power-enable VIH threshold throughout at least 100 software resets, with no low pulse capable of dropping the Linux rail.

