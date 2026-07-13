# Legacy Linux transaction sequence

No Linux uploader or captured transaction log is present in this checkout.
Write/STOP versus repeated-start behavior, retry policy, fixed read lengths,
and ERASE address semantics are therefore unproven. The production build keeps
I2C protocol processing disabled and does not claim Linux OTA compatibility.

Known immutable protocol facts are sync `AA 44 18`, little-endian payload
length, frame-number XOR `0xFF`, and payload CRC16 transmitted low byte first.

