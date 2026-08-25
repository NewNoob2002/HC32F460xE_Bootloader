# Application handover contract

Current source behavior is authoritative; see [current_status.md](current_status.md).

The current handover validates the vector, disables global interrupts, deinitializes the board-configured I2C slave, forces PB5 off, puts the external watchdog output at its safe inactive level, keeps PB3 asserted, stops and clears SysTick, disables and clears every NVIC IRQ, sets `SCB->VTOR` to `APP_FLASH_BASE`, loads MSP and branches to the validated Thumb reset handler. Update-command handover starts only after the complete ACK read reaches `SLAVE_TX_DONE`.

Boot reads RMU reset flags without clearing them, so the application can consume the original reset cause after handover.
