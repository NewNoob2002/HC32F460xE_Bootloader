# Current implementation status

Baseline: node 1 safety baseline and Phase 2 inbound transport boundary closed with target evidence on 2026-08-25. This document describes reachable code, not only the feature macros in `Core/Inc/boot_config.h`.

## Implemented and reachable

- HC32F460xE startup, clock, millisecond timebase, reset-cause capture, power hold, external TPL5010 service, status LED and RTT/EasyLogger logging.
- Application-vector validation and immediate application handover after a normal reset when the vector is valid.
- Software-reset update window and invalid-application Recovery mode.
- Board-configured Boot/Linux I2C1 slave transport on PA3/SCL and PA2/SDA at address `0x11`, 400 kHz, with interrupt-driven RX/TX buffers. PA8/PA9 I2C2 belongs to the Application and is not initialized by Boot.
- Legacy `AA 44 18` framing, sequence complement, payload bounds and CRC16.
- Transport-independent protocol parsing through byte and chunk input APIs; `Protocol/` no longer polls or includes the I2C BSP.
- Main-loop RX orchestration through a fixed 544-byte transport buffer and `bsp_i2c_slave_read()`, which owns the RX ring critical section.
- HANDSHAKE, gated Application-region erase/download, per-chunk Flash readback comparison and gated jump after the ACK read completes.
- I2C address/byte/STOP/NACK/overflow/empty-read/busy/recovery counters and parser-timeout counters, with fault-triggered main-loop logging.
- I2C recovery only after an active transaction or hardware BUSY has made no progress for 2000 ms.
- HC32 ICG startup words are linked at `0x00000400`; the linker rejects a missing or incorrectly sized eight-word ICG block.
- CMake Debug, Release, ReleaseNoLog and HostTests presets.

## Feature-gate baseline

`ERASE` and `DOWNLOAD` dispatch require both their Legacy command macro and `BOOT_ENABLE_FLASH_UPDATE`. `JUMP` requires both `BOOT_ENABLE_LEGACY_JUMP` and `BOOT_ENABLE_APPLICATION_JUMP`. Simulation exercises enabled command state transitions without calling the Flash erase/write/read BSP.

## Current boot decision

| Application vector | Software reset | Mode |
|---|---|---|
| invalid | either | Recovery |
| valid | yes | ten-minute update window |
| valid | no | immediate application handover |

The update timeout is currently polled in both update-window and Recovery modes. After expiry it waits one second and deasserts PB3 power hold; if power remains present, the condition remains true on subsequent polls.

## Current safety and completeness limits

- Image validity is only the initial MSP and reset-vector check. There is no complete-image length, CRC/hash, signature, version, anti-rollback or persistent VALID/UPDATING metadata.
- A valid-looking vector written after erase is enough for `JUMP_TO_APP`; contiguous/full-image coverage is not required.
- Immediate repeated-start reads remain unsupported; the host must end the write with STOP, wait for processing, then issue a separate read.
- Stalled-I2C recovery and application handover ordering still require target/HIL evidence. The normal final-byte NACK+STOP read-completion path is verified.
- Metadata A/B sectors are reserved in the map but unused.
- No current hardware/HIL run proves Flash update, power-loss behavior, I2C retry behavior, GPIO waveforms or application handover.
- Outbound response reservation/publication remains I2C-specific Phase 3 scope.

## Automated verification

Host CTest covers CRC, parser rejection/recovery/timeout, maximum frames, null and zero-length chunk input, frames split across chunks, multiple frames per chunk, Legacy golden bytes, enabled/disabled/simulation gates, TX busy ownership, ACK-before-jump, mocked Flash failures, memory ranges, vector validation and boot-mode selection. GitHub Actions builds host tests plus Release and ReleaseNoLog firmware. Host mocks do not prove target Flash, interrupts, timing or electrical behavior.

## Phase 2 evidence

- `Protocol/` accepts ordered byte chunks and has no BSP/I2C, raw RX-buffer or slave-state dependency.
- `main.c` owns BSP read -> parser chunk feed -> partial-frame timeout -> I2C recovery ordering.
- Debug, Release and ReleaseNoLog clean builds pass; Debug BSS is 12,984 bytes after replacing the parser-owned 1024-byte buffer with the 544-byte main-loop transport buffer.
- Debug BIN SHA-256: `819262842eb0a550be33687cddd2822e41c5fe950846a3067861b811b3c16ec4`; Debug ELF SHA-256: `d5d6af9bd11eb5c13682546bf6e8a057c9d4809d2f8203794b51a177fc0a93fc`.
- Debug, Release and ReleaseNoLog retain `.icg_sec` at `0x00000400`, size `0x20`, with the validated eight words. Host CTest passes 4/4.
- After a fresh reset, the exact I2C1 address `0x11`, 400 kHz HANDSHAKE returned `AA 44 18 01 FE 0C 00 20 00 00 00 00 00 00 00 00 00 00 00 A0 6E`.
- Post-read capture recorded RX/TX address matches `1/1`, RX/TX bytes `21/21`, one completed response read, released TX ownership, `PRIMASK=0`, no exception and no overflow, empty-read, busy, arbitration or recovery event.
- A preserved first attempt ran about 35 minutes 56 seconds after reset, beyond the ten-minute window, and returned 21 controlled `0xFF` bytes while the complete request remained queued. The passing retry was performed after reset within the documented window; the failure was not converted into a pass.

## Node 1 target evidence

- Final logging-enabled Debug image SHA-256: `0187072b1a1e4c582dda6a69b010179c8059b2b82ba5dad305040935de14dbde`.
- HC32F460xE pin-revised test board, I2C1 PA3/SCL and PA2/SDA, address `0x11`, 400 kHz: the 21-byte HANDSHAKE response matched exactly.
- Post-read capture recorded one RX and one TX address match, 21 RX bytes, 21 TX bytes, one completed response read, released TX ownership, `PRIMASK=0`, no active exception and no overflow, empty-read, busy, arbitration or recovery events.
- Release and ReleaseNoLog place `.icg_sec` at `0x00000400`, size `0x20`, with words `FFDFFFBF FFFFFEFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF`.
- Host CTest passed 4/4. Raw lab transcripts and safety preflights remain local under ignored `debug_artifacts/`; reusable J-Link commands live in `Tools/jlink/`.
