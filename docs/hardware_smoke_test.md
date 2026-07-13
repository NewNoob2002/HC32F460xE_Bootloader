# Hardware smoke test

No hardware tests were executed in this environment.

1. Software-reset power hold: probe PB3, reset, and Linux rail; repeat 100 times. Require no PB3 low pulse and no Linux reboot.
2. Watchdog: blocked until polarity/pulse/device are confirmed. Then probe PA6 for idle level, active level, pulse width, 3000 ms interval, recovery feeding, and long-run reset freedom.
3. Debug: after PSPCR `0x03`, verify PB3 GPIO control, SWD attachment, RTT output, reset reconnect, and connect-under-reset recovery.
4. LED: blocked until polarity/topology are confirmed. Then verify all logical patterns and off-before-jump.
5. Logging: verify compact reset/power/watchdog/LED/app/mode records and confirm disconnected-debugger boot never blocks.
6. Application jump: verify PB3 remains asserted, PA6 remains in its documented state, PB5 is off when enabled, SysTick is disabled, and the application starts.

