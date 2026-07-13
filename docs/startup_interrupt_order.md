# Startup and interrupt order

The reset path is `Reset_Handler -> SystemInit -> __libc_init_array -> main`.
`SystemInit` grants full CP10/CP11 access, executes DSB then ISB, writes VTOR,
and again executes DSB then ISB. It does not execute `cpsie i`.

On Cortex-M reset, PRIMASK is normally zero. The live target therefore showed
PRIMASK `0` at both `SystemInit` and `main`; this does not mean the removed
early `__enable_irq()` still exists. No peripheral IRQ is enabled until its
software state and callback have been installed.

For I2C1, initialization follows this order:

1. Clear the two RX transactions, TX state, counters, and direction state.
2. Enable the I2C1 clock and configure PA3/PA2.
3. Deinitialize and initialize `CM_I2C1`.
4. Configure slave address `0x11`.
5. For INT005/INT006/INT004, call `INTC_IrqSignIn`, clear NVIC pending state,
   set priority, then enable that NVIC channel.
6. Enable I2C1 and its address-match/RX-full events.

The callbacks do transport work only. They do not log, parse, erase/program
Flash, select Boot mode, or jump to the application.

The first project-owned `cpsie i` instructions are the paired restores after
short critical sections in `bsp_i2c_slave_take_rx_transaction()` and
`bsp_i2c_slave_publish_response()`. They are not startup interrupt enables.
Future cleanup should restore the saved PRIMASK instead of unconditionally
enabling interrupts if these APIs are ever called from a masked context.

Live evidence is in
[`gdb_startup_runtime.log`](../debug_artifacts/jlink/gdb_startup_runtime.log).

