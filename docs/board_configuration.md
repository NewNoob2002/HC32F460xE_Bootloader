# Board configuration

| Function | Pin | GPIO mode | Electrical contract |
| --- | --- | --- | --- |
| `MCU_ON_OFF_BAT_POWER` | PB3 | CMOS push-pull output | Active high; held high throughout Boot |
| TPL5010 `WDOG_DONE` | PA6 | CMOS push-pull output | Idle low; active-high 1 ms pulse every 3000 ms |
| `NET_STATE` LED gate | PB5 | CMOS push-pull output | High turns 2N7002/LED on; low turns it off |
| Main crystal | PH0/PH1 | Analog crystal pins | Existing clock configuration |

PB3 is also JTDO/TRACESWO. Boot preloads/configures PB3 high before writing PSPCR `0x0003`, which retains PA13/PA14 SWDIO/SWCLK and releases the other JTAG/debug pins. PA6 has no relevant debug-pin conflict.

Protection uses `LL_PERIPH_WE(CONFIG_PERIPH_WE)` and restores the supplied `CONFIG_PERIPH_WP` mask after board initialization. Compile-time assertions enforce watchdog and LED level/timing consistency.

