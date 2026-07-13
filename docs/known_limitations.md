# Known limitations

- The legacy watchdog and LED implementations are not present, so PA6/PB5 electrical activation is deliberately disabled.
- External-watchdog device, idle/active levels, pulse width, and first-feed timing require schematic, source, or scope confirmation.
- LED polarity and output topology require confirmation.
- SWD/RTT survival after PSPCR `0x03` is supported by register definitions but not physically verified.
- The minimal clock code observes the reset-state clock; it does not import the legacy 200 MHz PLL sequence.
- Hardware power-hold glitch behavior is unverified despite the latch-preload sequence.
- Protocol, I2C, Flash update, and complete OTA state logic remain disabled.

