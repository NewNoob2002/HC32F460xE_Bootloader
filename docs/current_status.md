# Current implementation status

Baseline: source on `main` as reviewed on 2026-08-23. This document describes reachable code, not only the feature macros in `Core/Inc/boot_config.h`.

## Implemented and reachable

- HC32F460xE startup, clock, millisecond timebase, reset-cause capture, power hold, external TPL5010 service, status LED and RTT/EasyLogger logging.
- Application-vector validation and immediate application handover after a normal reset when the vector is valid.
- Software-reset update window and invalid-application Recovery mode.
- I2C1 slave transport at address `0x11`, 400 kHz, with interrupt-driven RX/TX buffers.
- Legacy `AA 44 18` framing, sequence complement, payload bounds and CRC16.
- HANDSHAKE, full Application-region erase, addressed Application download, per-chunk Flash readback comparison and jump request after the ACK read completes.
- CMake Debug, Release, ReleaseNoLog and HostTests presets.

## Important configuration mismatch

`BOOT_ENABLE_FLASH_UPDATE`, `BOOT_ENABLE_LEGACY_ERASE`, `BOOT_ENABLE_LEGACY_DOWNLOAD` and `BOOT_ENABLE_LEGACY_JUMP` are currently zero, but the command switch in `boot_update_service.c` is unconditional. The production image therefore still contains and can reach real `EFM_SectorErase` and `EFM_Program` operations. Until the gates are wired into dispatch, these macros must not be treated as safety controls.

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
- The parser has a timeout API, but `main()` does not call it; `protocol_last_byte_ms` is assigned but not consumed.
- The I2C TX buffer can be overwritten before the previous response is read, and `txBufferWrite()` has no length/busy guard.
- The handover implementation disables global IRQs, suspends SysTick, deinitializes I2C, sets MSP and branches. It does not currently relocate VTOR or call the watchdog/LED/power handover helpers described by older contracts.
- Metadata A/B sectors are reserved in the map but unused.
- No current hardware/HIL run proves Flash update, power-loss behavior, I2C retry behavior, GPIO waveforms or application handover.

## Automated verification

Host CTest covers CRC, parser rejection/recovery, maximum frames, protocol responses, mocked Flash failures, memory ranges, vector validation and boot-mode selection. GitHub Actions builds host tests plus Release and ReleaseNoLog firmware. Host mocks do not prove target Flash, interrupts, timing or electrical behavior.
