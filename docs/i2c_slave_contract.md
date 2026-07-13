# I2C1 slave contract

I2C1 uses PA03/SCL function 49 and PA02/SDA function 48, seven-bit address
`0x11`, `I2C_CLK_DIV2`, 400 kHz, and SCL time 5. EEI is registered on INT005 at
priority 9, RXI on INT006 at priority 10, and TEI on INT004 at priority 10.
Registration failures, including occupied programmable channels, make init
fail and put Boot in its powered/watchdog-serviced fatal mode.

Two 544-byte RX transaction buffers are owned alternately by ISR and main-loop
code. STOP commits a transaction; overflowed transactions are discarded. The
streaming parser retains partial frames across STOP boundaries and resets after
250 ms without another byte.

The 544-byte TX response is published atomically and cannot be overwritten
while unread. Each transmitter address match starts at byte zero. Over-read
returns `0xFF`; STOP/NACK retires complete or partial reads deterministically.

Linux compatibility is a write transaction ending in STOP, a short processing
delay, then a separate read transaction. Immediate repeated-start response is
not required.

Callbacks only move bytes, update indexes/events, and clear I2C flags. They do
not parse, calculate frame CRC, log, access Flash, or change Boot state.

