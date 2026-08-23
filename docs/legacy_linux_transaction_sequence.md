# Legacy Linux transaction sequence

Current implementation status: [current_status.md](current_status.md).

No Linux uploader or captured transaction log is present in this checkout. The implemented firmware contract is a write transaction ending in STOP, polling time to parse/execute, then a separate read transaction for the response. Immediate repeated-start response is not guaranteed.

The reachable command order is `HANDSHAKE -> ERASE_FLASH -> APP_DOWNLOAD... -> JUMP_TO_APP`. ERASE currently requires address `0x00008000`, DOWNLOAD uses absolute little-endian Flash addresses, and JUMP is armed only after its ACK has been queued and consumed after I2C TX completion. Host retry policy, fixed read lengths, erase timeout and interrupted-update recovery remain unproven.

Known immutable protocol facts are sync `AA 44 18`, little-endian payload
length, frame-number XOR `0xFF`, and payload CRC16 transmitted low byte first.
