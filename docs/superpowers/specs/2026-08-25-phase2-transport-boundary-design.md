# Phase 2 Transport Boundary Design

## Status

Approved in-chat design for branch `feat/portability-node2-transport-boundary`, based on node 1 commit `16f72f5`.

## Goal

Remove all I2C/BSP polling responsibilities from the legacy protocol parser while preserving the HC32 I2C1 wire behavior, response ownership rules, recovery behavior and protocol golden bytes established by node 1.

Phase 2 creates an inbound transport boundary only. The parser accepts bytes or byte chunks; the main loop owns transport polling and feeds received bytes into the parser. Outbound response publication remains on the existing I2C-specific path until Phase 3 separates protocol encoding and update execution.

## Current coupling

`BootProtocolParserProcess()` currently includes `bsp_critical.h` and `bsp_i2c_slave.h`, examines `SLAVE_RX_DONE`, drains the I2C RX ring into a parser-owned 1024-byte global buffer and then calls `BootProtocolParserPushByte()`. This makes the protocol library depend on one transport, one transaction-state enum and one interrupt-protection mechanism.

The main loop already owns parser timeout and I2C recovery scheduling, so transport polling belongs there rather than in the parser.

## Chosen approach

Use the smallest boundary that proves transport-independent parsing:

1. The protocol parser exposes a chunk-feeding API built on the existing byte-feeding API.
2. The I2C BSP exposes one critical-section-safe batch read operation.
3. `main.c` owns a fixed RX block buffer and orchestrates BSP read, parser feed, timeout and recovery.
4. Existing I2C TX reservation and publication APIs remain unchanged for Phase 3.

Do not add a transport vtable, function-pointer registry, dynamic allocation, multi-transport selection or unused `write`/`tx_idle` abstraction in this phase.

## Interfaces

### Protocol chunk input

Add to `Protocol/Inc/boot_protocol_parser.h`:

```c
size_t BootProtocolParserPushBytes(boot_protocol_parser_t* parser,
                                   const uint8_t* bytes,
                                   size_t length);
```

Contract:

- Return `0U` when `parser` or `bytes` is `NULL`, or when `length` is zero.
- Process every supplied byte in order by calling `BootProtocolParserPushByte()`.
- Return the number of complete valid frames produced while consuming the chunk.
- Preserve partial parser state across calls so a frame may span arbitrary chunk boundaries.
- Permit multiple complete frames in one chunk.
- Preserve all existing byte, frame, CRC, length, timeout and callback statistics.

Remove `BootProtocolParserProcess()`. The parser source must not include BSP headers or refer to I2C buffers, slave states or critical sections.

### I2C batch read

Add to `Drivers/BSP/Inc/bsp_i2c_slave.h`:

```c
size_t bsp_i2c_slave_read(uint8_t* buffer, size_t capacity);
```

Contract:

- Return `0U` for a `NULL` buffer, zero capacity or when no stopped RX transaction is ready.
- Enter the BSP critical section, recheck the RX-ready state and copy up to `capacity` bytes from the RX ring.
- Return the copied byte count and leave any excess bytes queued for the next call.
- Keep ISR-owned ring indices and overflow accounting inside the I2C BSP.
- Do not parse protocol data, update parser timeouts or publish responses.

The existing internal ring helpers may remain only where the driver or current host mocks still require them. No new public raw-ring API is added.

## Main-loop data flow

Add a file-static buffer to `Core/Src/main.c`:

```c
static uint8_t transport_rx_buffer[BOOT_I2C_BUFFER_CAPACITY];
```

Each loop iteration preserves the node 1 ordering:

```text
bsp_i2c_slave_read
        | bytes > 0
        v
BootProtocolParserPushBytes
        |
        +--> update protocol_last_byte_ms
        |
partial-frame timeout check
        |
bsp_i2c_slave_poll / recovery
        | recovery
        +--> BootProtocolParserReset
```

`BOOT_I2C_BUFFER_CAPACITY` is 544 bytes, which is large enough to drain the current RX ring in one main-loop pass. Removing the parser-owned 1024-byte buffer and adding this 544-byte buffer reduces static RAM use by approximately 480 bytes.

## Concurrency and ownership

- RX ring mutation remains split between the I2C RX ISR producer and the main-loop consumer. `bsp_i2c_slave_read()` owns the critical section needed to snapshot and advance the ring safely.
- The parser executes only in main-loop context and therefore needs no interrupt masking.
- TX reservation, unread-response protection, TEI byte loading and `SLAVE_TX_DONE` ownership remain unchanged.
- I2C recovery still runs after RX feeding and timeout handling. A recovery event resets only partial parser state, as in node 1.
- Diagnostic counter snapshots remain logging-only and may span ISR updates, as documented in node 1.

## Error behavior

- Invalid parser chunk arguments are no-ops returning zero completed frames.
- Invalid BSP read arguments are no-ops returning zero bytes.
- A destination buffer smaller than queued RX data produces a bounded partial read; remaining bytes stay queued.
- RX overflow, stalled transaction and hardware BUSY behavior are unchanged and remain accounted by the I2C BSP.
- Parser rejection, CRC failure, length failure and caller-managed timeout behavior are unchanged.

## Files in scope

- `Protocol/Inc/boot_protocol_parser.h`
- `Protocol/Src/boot_protocol_parser.c`
- `Drivers/BSP/Inc/bsp_i2c_slave.h`
- `Drivers/BSP/Src/bsp_i2c_slave.c`
- `Core/Src/main.c`
- `Tests/boot_protocol_parser_tests.c`
- `docs/current_status.md`
- `docs/i2c_slave_contract.md`
- `docs/portability_plan.md`

Build-system changes are permitted only if required to remove an obsolete parser-to-BSP include dependency. No new production source module is planned.

## Tests

Host tests must cover:

1. A valid frame supplied in one chunk.
2. A valid frame split across several chunks.
3. Multiple valid frames supplied in one chunk.
4. Noise and an invalid frame followed by a valid frame across chunk boundaries.
5. `NULL` parser, `NULL` byte pointer and zero-length input.
6. Existing legacy HANDSHAKE golden bytes and enabled, disabled and simulated update-service configurations.

Verification commands must include:

```sh
cmake --preset Debug
cmake --build build/Debug --clean-first --parallel
cmake --preset Release
cmake --build build/Release --clean-first --parallel
cmake --preset ReleaseNoLog
cmake --build build/ReleaseNoLog --clean-first --parallel
cmake --preset HostTests
cmake --build build/HostTests --clean-first --parallel
ctest --test-dir build/HostTests --output-on-failure
```

Dependency checks must prove that `Protocol/` contains no `bsp_*`, `rxBuffer*`, `SLAVE_*` or I2C references. Debug, Release and ReleaseNoLog must retain the node 1 ICG block at `0x00000400`, size `0x20`.

## Target acceptance

After software gates pass, flash the resulting logging-enabled Debug BIN with the reusable J-Link flow and repeat the node 1 I2C1 test at address `0x11`, 400 kHz:

```text
Write with STOP:
AA 44 18 01 FE 0C 00 20 11 00 00 00 00 00 00 00 00 00 00 D4 E3

Wait 5-20 ms, then perform a separate 21-byte read.

Expected:
AA 44 18 01 FE 0C 00 20 00 00 00 00 00 00 00 00 00 00 00 A0 6E
```

The post-read state must again show 21 RX bytes, 21 TX bytes, one completed response read, released response ownership and no overflow, empty-read, busy, arbitration or recovery event.

## Non-goals

- Generic transport registration or runtime transport selection.
- UART, USB, CAN, Ethernet or BLE implementation.
- Moving response encoding or Flash/session execution out of `BootUpdateService`.
- Replacing I2C TX reservation with a new generic ownership model.
- Logical image offsets, metadata, whole-image verification or rollback policy.
- Application-handover refactoring or additional handover HIL.

These remain assigned to later portability phases.

## Completion criteria

Phase 2 is complete when the parser library is transport-independent, all host/build/dependency checks pass, the exact 400 kHz HC32 HANDSHAKE remains green on target and the branch documents the remaining outbound I2C coupling as Phase 3 scope.
