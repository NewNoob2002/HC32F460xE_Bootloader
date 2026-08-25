# I2C slave contract

Current implementation status: [current_status.md](current_status.md).

The current Boot board configuration selects I2C1 on PA03/SCL function 49 and PA02/SDA function 48. The BSP consumes the peripheral instance, clock, baudrate, SCL time, pins, pin functions, IRQ channels, IRQ sources and priorities from `bsp_board_config.h` instead of embedding them in the driver. The slave uses seven-bit address
`0x11`, `I2C_CLK_DIV2`, 400 kHz, and SCL time 5. EEI is registered on INT005 at
priority 9, RXI on INT006 at priority 10, and TEI on INT004 at priority 10.
Registration failures, including occupied programmable channels, make init
fail and put Boot in its powered/watchdog-serviced fatal mode.

One 544-byte RX ring and one 544-byte TX buffer are shared between ISR and main-loop code. STOP changes the state to `SLAVE_RX_DONE`; the parser copies available RX bytes inside a short critical section. RX overflow drops new bytes and increments a counter. The parser retains partial frames across STOP boundaries, and `main()` drops a partial frame after 250 ms without a new byte.

The update service reserves response ownership before side effects. `txBufferWrite()` rejects NULL, zero/oversized writes, active TX and unread responses. On a read address match, the driver writes the first DTR byte before clearing the address flag, then enables the TX-complete/TEI path. TEI preloads each following byte after the preceding transfer completes; a separate count records bytes loaded into DTR so the final master NACK completes the response without requiring another TEI. A partial or arbitration-lost read rolls the TX cursor back to the start of that transaction so the next independent read receives the complete response; only a STOP after the full response was clocked enters `SLAVE_TX_DONE`. An empty master read returns controlled `0xFF` bytes until the master ends the transaction. The update service consumes a jump request only in `SLAVE_TX_DONE`.

Linux compatibility is a write transaction ending in STOP, a short processing
delay, then a separate read transaction. Immediate repeated-start response is
not required.
PA08/PA09 I2C2 belongs to the Application and is not initialized by Boot. The
MP2762 and battery-pack gauge observed on that separate bus do not constrain the
Boot/Linux I2C1 bitrate.

Callbacks move bytes, update state/counters/register snapshots and clear I2C flags; NACK and STOP are independent checks so both can be handled in one EEI. Address match and STOP also clear the latched START flag so a completed transaction cannot leave hardware BUSY asserted. Parsing, CRC, Flash operations and diagnostic logging run in polling context. Address match, RX/TX byte, NACK and STOP advance progress. `bsp_i2c_slave_poll(now_ms)` reinitializes hardware/IRQs/RX and active TX transaction state only when an active transaction or hardware BUSY has made no progress for 2000 ms; an idle bus and an unread ACK do not trigger recovery. A published response survives recovery and is rolled back to the current transaction start before hardware reset. Recovery records SR/state/reason and causes the main loop to reset parser partial state.
