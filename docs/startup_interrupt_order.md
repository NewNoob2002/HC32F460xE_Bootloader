# Startup and interrupt order

Current implementation status: [current_status.md](current_status.md).

The reset path is `Reset_Handler -> SystemInit -> __libc_init_array -> main`.
`SystemInit` grants full CP10/CP11 access, executes DSB then ISB, writes VTOR,
and again executes DSB then ISB. It does not execute `cpsie i`.

On Cortex-M reset, PRIMASK is normally zero. The live target therefore showed
PRIMASK `0` at both `SystemInit` and `main`; this does not mean the removed
early `__enable_irq()` still exists. No peripheral IRQ is enabled until its
software state and callback have been installed.

For I2C1, current initialization follows this order:

1. Enable the I2C1 clock and configure PA3/PA2.
2. Deinitialize and initialize `CM_I2C1`.
3. Configure slave address `0x11`.
4. For INT005/INT006/INT004, call `INTC_IrqSignIn`, clear NVIC pending state,
   set priority, then enable that NVIC channel.
5. Enable I2C1 and its address-match/RX-full events.
6. Reset the RX/TX buffers and error counter.

The callbacks do transport work only. They do not log, parse, erase/program
Flash, select Boot mode, or jump to the application.

The parser briefly masks interrupts while copying RX bytes through `bsp_enter_critical()` and restores the captured PRIMASK through `bsp_exit_critical()`. Application handover later disables global interrupts and does not re-enable them before branching.

Live evidence is in
[`gdb_startup_runtime.log`](../debug_artifacts/jlink/gdb_startup_runtime.log).
