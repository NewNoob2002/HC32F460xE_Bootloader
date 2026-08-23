# Application handover contract

Current source behavior is authoritative; see [current_status.md](current_status.md). Before a validated jump, Boot deinitializes I2C1, disables global interrupts, suspends SysTick, reloads MSP from `0x00008000`, and branches to the Thumb reset handler.

The current handover does **not** explicitly relocate VTOR, clear every NVIC pending source, stop the external-watchdog scheduler, force PB5 off, or reassert PB3. Those operations are required before this can be treated as a complete cross-application contract. The application must initialize its vector base and inherited peripherals defensively and assume TPL5010 service ownership promptly.

Boot reads RMU reset flags without clearing them, so the application can consume the original reset cause after handover.
