# Recovered legacy upload contract

This document separates byte-for-byte evidence from unknown behavior. It is not
a proposal for a new OTA architecture.

## Proven frame contract

```text
0       AA
1       44
2       18
3       frame number
4       frame number XOR FF
5..6    payload length, little-endian
7..     payload
7+N     CRC low byte
8+N     CRC high byte
```

The CRC covers only the payload. `CRC("123456789") == 0x31C3`, transmitted as
`C3 31`. Payload limits are 12 through 524 bytes; data starts at payload offset
12/complete-frame offset 19 and is at most 512 bytes.

Commands and ACK values are exactly those in `LegacyReference/liteParse.h`:

```text
20 HANDSHAKE       00 OK
21 JUMP_TO_APP     01 ERROR
22 APP_DOWNLOAD    02 ABORT
23 APP_UPLOAD      03 TIMEOUT
24 ERASE_FLASH     04 ADDR_ERROR
```

## Proven command dispatch

The legacy I2C polling function feeds bytes to `p16_parse_byte`. On a valid CRC,
the global callback calls `message_decode`, and the returned response is placed
in the I2C TX buffer. Linux-visible responses are therefore prepared only after
the synchronous command handler returns.

The legacy RX polling loop contains a half-processing defect because
`rxBufferAvailable()` is reevaluated while `rxBufferRead()` decreases it. The
current transaction-buffer transport must be retained; reproducing that defect
is not part of compatibility.

## HANDSHAKE (`0x20`)

Proven behavior:

1. Copy the complete validated request.
2. Change complete-frame byte 8 to `ACK_OK`.
3. Preserve payload and complete-frame length.
4. Recalculate the CRC across the payload and write low byte first.
5. Publish the response after parsing in the polling context.

The current codec matches the required 21-byte golden response exactly.

## ERASE_FLASH (`0x24`)

Proven behavior:

- Address is decoded little-endian from complete-frame bytes 9..12 using an
  unsafe unaligned cast in the old code.
- Address validity is considered only when request type byte 8 is DATA (`0x12`).
- The old start-address check was `address >= EFM_BASE + BOOT_SIZE` and
  `address < EFM_BASE + EFM_END_ADDR`.
- `FLASH_PageNumber(address)` determines the number of sectors.
- Erasure starts at sector index `BOOT_SIZE / EFM_SECTOR_SIZE` and advances one
  sector at a time.
- The request address is stored in `u32FlashAddr_Erase`.
- ACK is prepared after the erase loop finishes.
- The response payload is 12 bytes with OK, ERROR, or ADDR_ERROR and a new CRC.
- The `APP_STATUS_SECTOR` erase is commented out.

Unknown and blocking:

- The semantic meaning of the address field.
- The implementation/formula of `FLASH_PageNumber`.
- Whether the uploader sends image end, size, last address, or another value.
- The exact timeout allowed by Linux for the synchronous erase response.
- The implementation of the sector-index-based `FlashEraseSector` wrapper.

The current DDL instead exposes `EFM_SectorErase(uint32_t address)` with an
8-KiB sector size. Substituting `index * 0x2000` is mechanically plausible but
does not recover the missing erase-count contract and is therefore not enabled.

## APP_DOWNLOAD (`0x22`)

Proven behavior:

- Address is absolute: the old handler passes it unchanged to `FlashWritePage`.
- Data length is `payload_length - 12`.
- Data begins at complete-frame offset 19.
- One frame can contain up to 512 data bytes.
- Programming is synchronous and ACK is prepared afterward.
- Response payload is reduced to 12 bytes and returns OK, ERROR, or ADDR_ERROR.

Not present in the old code:

- Whole-range validation; it checked only the starting address.
- Explicit sequential, duplicate, or retry policy.
- Explicit readback verification.
- Whole-image verification.

The old `FlashWritePage` implementation is absent, so its alignment and final
short-packet behavior cannot be proven. Current DDL `EFM_Program` accepts a byte
length, requires a word-aligned destination, and pads a partial final word with
`0xFF`, but that vendor behavior alone does not prove the old wrapper contract.

## JUMP_TO_APP (`0x21`)

Proven behavior:

1. Assign `APP_FLAG` to a local 32-bit value.
2. Call `FlashWritePage(BOOT_PARA_ADDRESS, &value, 4)` and ignore its return.
3. Copy the complete request and set result byte 8 to OK.
4. Recalculate payload CRC and return the original frame length.
5. Set global `BOOT_DELAY_TIME = 50`.

The available handler does not validate the vector and does not branch or reset
directly. The missing old main loop must have interpreted `BOOT_DELAY_TIME`.
Neither its units nor the relationship between response transmission and jump
is present. `APP_FLAG` and `BOOT_PARA_ADDRESS` are undefined in every repository
revision. This command cannot be compatibly or safely enabled.

## APP_UPLOAD (`0x23`)

Only the command constant exists. There is no legacy switch handler, so the
default path returns no response. It remains disabled.

## Linux transaction sequence

No Linux uploader, captured trace, service unit, binary, or source referencing
this protocol exists in the repository or surrounding workspace. The only
recoverable sequence is the expected order stated by the firmware task:

```text
HANDSHAKE -> ERASE_FLASH -> APP_DOWNLOAD... -> JUMP_TO_APP
```

ERASE address content, packet ordering, retry behavior, response lengths used by
the host, erase timeout, and final ACK/jump timing remain unknown.

## Current mandatory safety boundary

Any future port must reject zero-length or overflowing writes and enforce:

```text
APP_FLASH_BASE <= address
address < 0x0007A000
length <= 0x0007A000 - address
```

This guard is not sufficient to recover ERASE or marker semantics; it only
prevents Boot/reserved-region corruption once those semantics are proven.

## Deliberately deferred

- Metadata
- UPDATING/VALID persistent state
- whole-image CRC32
- new image header
- power-loss-safe two-copy state
- new protocol commands or ACK values
- new Linux uploader behavior

