# Hardware smoke test

Hardware access was not available during this refactor. All board steps are pending manual execution.

1. Build: run the clean release commands in `build_and_flash.md`; confirm no errors, inspect warnings, check `.map`, and require Flash usage below 32 KiB.
2. Flash: use the board's confirmed HC32 tool/probe and 512 KiB algorithm; program/verify the ELF or HEX and reset. Confirm no reset loop.
3. Normal boot: with a valid application at `0x8000`, power-cycle and confirm the application starts.
4. Software-reset entry: trigger the legacy software-reset path and confirm boot remains in its update window.
5. I2C/handshake: first supply and review the missing SDA/SCL pin map, IRQ channel routing, command ID, offsets, ACK, and golden frames. Then enable the hardware/profile implementation and verify address `0x11` and byte-exact response with a logic analyzer.
6. Invalid application: with a development-only vector invalidation method, reset and confirm no application jump and continued recovery mode.

Never invalidate a production application without a recovery programmer and known-good image.

