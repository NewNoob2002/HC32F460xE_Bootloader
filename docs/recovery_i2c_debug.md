# Recovery I2C debug

The missing upload path had four independent causes:

1. `Drivers/BSP/Src/bsp_i2c_slave.c` was a placeholder whose init function
   always returned `false`.
2. The BSP, protocol sources, HC32 I2C DDL, interrupt DDL, and FCG DDL were not
   in the target CMake source list.
3. `LL_I2C_ENABLE` and `LL_INTERRUPTS_ENABLE` were disabled.
4. `main()` neither initialized nor polled I2C or the legacy protocol.

Recovery now initializes I2C before protected-register access is restored,
initializes the handshake service, and polls both indefinitely. Only a valid
software-reset update window runs the application timeout. A blank application
cannot set `jump_requested`.

The former uninitialized `boot_context_t context` is now zero initialized and
the application-valid flag, timeout origin, and jump request are assigned before
mode selection.

