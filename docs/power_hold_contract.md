# Power-hold contract

Current implementation status: [current_status.md](current_status.md).

PB3 drives `MCU_ON_OFF_BAT_POWER` active high as a CMOS push-pull output. A schematic pull-up assists during reset, but firmware does not rely on it.

The startup sequence unlocks protected registers, configures the debug-port ownership, initializes PB3 high/output and later restores write protection. The fatal loop reasserts PB3 and services watchdog/LED polling. `bsp_power_hold_assert()` is idempotent.

Normal startup and fatal mode retain PB3 high. `boot_timeout_poll()` intentionally deasserts PB3 after ten minutes in both update-window and Recovery modes. Application preparation currently does not explicitly reassert or deinitialize PB3. Hardware glitch performance remains pending oscilloscope validation over at least 100 resets.
