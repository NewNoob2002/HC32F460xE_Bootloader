# Current legacy protocol

Only these properties are confirmed by the task specification: sync `AA 44 18`, slave address `0x11`, maximum application payload 512 bytes, CRC-16/XMODEM check value `0x31C3` for `123456789` with wire order `C3 31`, and requirements to preserve frame number, its XOR byte, payload byte order, command IDs, ACK values, and legacy commands.

The repository contains no legacy parser or protocol files. Consequently command offsets/IDs, result/type offsets, address/data offsets, exact CRC coverage, full maximum frame size, handshake request/response bytes, erase/download/jump formats, and I2C transaction boundaries are `UNKNOWN`. Full compatibility cannot honestly be claimed until those sources or protocol captures are provided.

The new parser will bound all copies, accept fragmented input, reject malformed lengths/CRC, and expose only a handshake dispatch seam. Flash erase/program and application jumping are not parser actions.

