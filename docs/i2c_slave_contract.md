# I2C1 slave contract

Current implementation status: [current_status.md](current_status.md).

I2C1 uses PA03/SCL function 49 and PA02/SDA function 48, seven-bit address
`0x11`, `I2C_CLK_DIV2`, 400 kHz, and SCL time 5. EEI is registered on INT005 at
priority 9, RXI on INT006 at priority 10, and TEI on INT004 at priority 10.
Registration failures, including occupied programmable channels, make init
fail and put Boot in its powered/watchdog-serviced fatal mode.

One 544-byte RX ring and one 544-byte TX buffer are shared between ISR and main-loop code. STOP changes the state to `SLAVE_RX_DONE`; the parser copies available RX bytes inside a short critical section. RX overflow silently drops new bytes. The parser retains partial frames across STOP boundaries, but the configured 250 ms timeout is not connected in `main()`.

`txBufferWrite()` copies without a capacity or busy check and resets the TX indexes. An unread response can therefore be overwritten by a later command. TX completion is recognized on STOP after a slave-read transaction; the update service waits for this state before consuming a jump request.

Linux compatibility is a write transaction ending in STOP, a short processing
delay, then a separate read transaction. Immediate repeated-start response is
not required.

Callbacks move bytes, update state/counters and clear I2C flags. Parsing, CRC and Flash operations run in polling context. Error-count recovery reinitializes I2C after 2000 recorded errors, although the present callbacks do not visibly increment that counter.
