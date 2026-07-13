# Board configuration

The board reference is `LegacyReference/mcu_config.h`. It confirms PH0/PH1 for the external crystal, active-high PB3 power control, PA6 external-watchdog feed, PB5 function LED, and a 3000 ms feed interval. The reference directory does not contain the legacy GPIO initialization, watchdog waveform, or LED implementation.

| Function | HC32 DDL encoding | Electrical contract |
| --- | --- | --- |
| Power hold | Port B, `GPIO_PIN_03` | Active high, CMOS push-pull |
| External watchdog | Port A, `GPIO_PIN_06` | Polarity and pulse width UNKNOWN; hardware activation gated |
| Status LED | Port B, `GPIO_PIN_05` | Polarity UNKNOWN; hardware activation gated |
| Main crystal | Port H, `GPIO_PIN_00 | GPIO_PIN_01` | Legacy clock reference marks both analog |

The power sequence uses `LL_PERIPH_WE`, `GPIO_SetPins`, `GPIO_StructInit`, `GPIO_Init`, and `GPIO_ReadOutputPins`. `CONFIG_PERIPH_WE` unlocks GPIO, EFM, FCG, PWC/CLK/RMU, and SRAM. The legacy `CONFIG_PERIPH_WP` mask locks only FCG and SRAM afterward; GPIO, EFM, and PWC/CLK/RMU remain writable, matching the supplied behavior rather than treating the masks as inverses.

