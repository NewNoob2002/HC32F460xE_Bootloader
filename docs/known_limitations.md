# Known limitations

- The checkout does not contain the named legacy protocol/I2C sources. Exact frame offsets, command IDs, ACK values, response bytes, and Linux transaction pattern remain `UNKNOWN`; handshake dispatch is intentionally disabled rather than guessed.
- I2C1 SDA/SCL pins, alternate functions, timing, and programmable NVIC channel assignments are absent. The bounded ISR/main transport exists, but `bsp_i2c_slave_init()` returns false and does not touch hardware.
- Watchdog and LED are disabled because no board contract or safe pin is confirmed.
- The update timeout is a polling-count placeholder, not a calibrated timebase.
- No Flash erase or programming code exists, as required for this baseline.
- Hardware flashing, reset behavior, application boot, I2C response, and handshake have not been physically tested.
- The repository provides no confirmed flash/debug command.

To complete the blocked compatibility work, provide the original `liteParse.c/.h`, `message_decode.c/.h`, `i2c.c/.h`, the board pin/clock configuration or schematic, and one captured valid request/response pair.

