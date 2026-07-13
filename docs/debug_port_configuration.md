# Debug-port configuration

Boot writes `CM_GPIO->PSPCR = 0x03` with `WRITE_REG16`, followed by DSB/ISB, once after GPIO protection is unlocked. The DDL maps PSPCR bits as TCK=bit0, TMS=bit1, TDO=bit2, TDI=bit3, and TRST=bit4. Therefore `0x03` keeps TCK/TMS active as SWCLK/SWDIO and releases TDO/SWO, TDI, and TRST to GPIO. SWD should remain enabled by register definition, so RTT memory access should remain possible, but this has not been tested on hardware.

The supplied statement that PA6 is multiplexed with JTAG conflicts with the HC32F460 pin table: PA6 has no JTAG/SWD function; PB3 is JTDO/TRACESWO. The legacy write is preserved exactly because it releases PB3 for power control while retaining the two-wire SWD pair.

Hardware test warning: verify RTT and reconnect after the write before relying on logging. If normal attach fails, use connect-under-reset, halt before board initialization, restore PSPCR/debug configuration, and reflash a known-good image.

