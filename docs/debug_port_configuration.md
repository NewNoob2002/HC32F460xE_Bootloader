# Debug-port configuration

Current implementation status: [current_status.md](current_status.md).

`bsp_debug_port_configure_for_boot_gpio()` writes `0x0003` to `CM_GPIO->PSPCR` with `WRITE_REG16`, then executes DSB and ISB. DDL definitions map bits 0/1 to TCK/TMS, also used as PA14 SWCLK and PA13 SWDIO. Bits 2/3/4 correspond to TDO/SWO, TDI, and TRST. Therefore `0x0003` retains two-wire SWD while releasing the other JTAG/debug pins.

PB3 is JTDO/TRACESWO and the safety-critical power-control GPIO. PA6 is not a conflicting debug pin. PB3 is preloaded and configured high before the PSPCR ownership configuration. SWD and RTT are expected to remain available but require physical verification. If attach fails, use connect-under-reset and reflash a known-good image.
