# Power-hold contract

Current implementation status: [current_status.md](current_status.md).

PB3 drives `MCU_ON_OFF_BAT_POWER` active high as a CMOS push-pull output. A schematic pull-up assists during reset, but firmware does not rely on it.

The startup sequence unlocks protected registers, configures the debug-port ownership, initializes PB3 high/output and later restores write protection. The fatal loop reasserts PB3 and services watchdog/LED polling. `bsp_power_hold_assert()` is idempotent.

Normal startup and fatal mode retain PB3 high. `boot_timeout_poll()` intentionally deasserts PB3 after ten minutes in both update-window and Recovery modes. Application handover reasserts PB3. After an upgrade response and its command-based 100 ms guard, Boot writes a magic/complement token in .noinit and requests NVIC_SystemReset(). It does not write SWRF directly. Startup consumes and clears the token before mode selection. Only a valid token plus software-reset cause and valid App bypass the update window; an invalid App enters Recovery, and ordinary software resets still enter the update window. The hardware SWRF is preserved for the application's power-on policy. The token is outside startup's BSS-clear range. Actual SRAM retention and PB3 power continuity during the reset remain product-validation items.
