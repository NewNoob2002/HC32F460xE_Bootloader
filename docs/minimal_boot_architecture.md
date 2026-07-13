# Minimal boot architecture

The baseline retains HC32 vendor startup, CMSIS/device headers, the root linker script, and the RMU DDL driver. Project code is split into four layers:

- `Core`: reset-to-main ordering, central options/memory map, state, and exception entry points.
- `Drivers/BSP`: reset flags, clock observation, safe-GPIO seam, watchdog seam, and interrupt-safe I2C transport buffers.
- `Protocol`: hardware-free CRC and bounded streaming parser. Flash and jump operations are absent.
- `App`: pure vector validator, boot decision, and the privileged application jump sequence.

Boot flow is reset capture/clear, safe GPIO seam, current-clock update, watchdog seam, vector validation, mode selection, then immediate application jump or polling mode. Software reset selects the update window. An invalid vector selects recovery indefinitely.

The link is constrained to `0x00000000..0x00007FFF`. The application remains at `0x00008000`. Jump cleanup disables interrupts, disables and clears all programmable NVIC channels, stops SysTick, deinitializes the boot I2C seam, relocates VTOR, sets MSP, and calls the Thumb reset handler.

No allocation and no Flash erase/program API are present. ISR-facing transport methods only append bytes or mark STOP/error; parsing is called from the main loop.

