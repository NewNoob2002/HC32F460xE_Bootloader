# Recovery I2C debug

Historical diagnosis with a current-status correction; see [current_status.md](current_status.md).

The missing upload path had four independent causes:

1. `Drivers/BSP/Src/bsp_i2c_slave.c` was a placeholder whose init function
   always returned `false`.
2. The BSP, protocol sources, HC32 I2C DDL, interrupt DDL, and FCG DDL were not
   in the target CMake source list.
3. `LL_I2C_ENABLE` and `LL_INTERRUPTS_ENABLE` were disabled.
4. `main()` neither initialized nor polled I2C or the legacy protocol.

Recovery now initializes I2C before protected-register access is restored, initializes the parser/update service, and polls them. Current `main()` calls `boot_timeout_poll()` for both update-window and Recovery modes, so a blank application also reaches PB3 deassertion after ten minutes. Recovery clears `context.jump_requested`, but the update-service jump request has a separate path after ACK transmission.

The former uninitialized `boot_context_t context` is now zero initialized and
the application-valid flag, timeout origin, and jump request are assigned before
mode selection.
