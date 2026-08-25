# Cross-platform bootloader plan

Status: phases 1 and 2 closed with target evidence on 2026-08-25; phases 3-8 have not started. The current HC32 behavior remains the baseline while boundaries are extracted incrementally.

## Objective

Reuse protocol parsing, upgrade policy and image management across MCU families. A new target should provide transport-facing BSP functions and a small platform port for Flash, time, reset and application handover.

```text
UART / USB / CAN / ETH / BLE / I2C
                |
        Upgrade Transport
       read / write / poll / tx_idle
                |
         Protocol Adapter
         Legacy DFU / SMP
                |
         Upgrade Manager
 begin / write / finish / abort / activate
                |
          Platform Port
 flash / time / reset / jump / watchdog
```

## Minimal boundaries

### Transport

Use one compile-time-selected transport first. Phase 2 introduces only the inbound `bsp_i2c_slave_read()` boundary and parser chunk input needed by the current product. Outbound `write`/`tx_idle` separation belongs to Phase 3; UART/USB/TCP can later pass byte chunks directly, while CAN or BLE adapters own required fragmentation/reassembly. Add multi-transport registration only when simultaneous transports are required.

### Protocol adapter

Each wire protocol owns framing, CRC and response encoding, and emits transport-neutral commands: `QUERY`, `BEGIN`, `WRITE`, `FINISH`, `ABORT` and `ACTIVATE`. Upgrade commands carry an image offset rather than an MCU absolute address. The current legacy adapter converts its absolute address field to an offset after range validation.

### Upgrade manager

Own session state, range/order rules, erase/write progress, whole-image verification, version policy, abort and activation. It must not include MCU headers or call transport/LED functions.

### Platform port

Provide only operations the manager actually needs: Flash erase/write/read, time, watchdog service, reset cause, handover preparation and jump. Memory base, capacity, erase size and write alignment are target configuration, not protocol constants.

## Migration phases

1. **Safety baseline (closed):** destructive feature gates control reachable dispatch; response ownership, parser timeout, stalled-I2C recovery, handover cleanup and HC32 ICG placement are enforced. Host regression and the logging-enabled 400 kHz I2C HANDSHAKE/NACK+STOP path have target evidence.
2. **Transport boundary (closed):** `Protocol/` accepts byte chunks without BSP/I2C dependencies; `main.c` owns a fixed 544-byte RX buffer and BSP read -> parser feed -> timeout -> recovery ordering; `bsp_i2c_slave_read()` owns RX critical-section handling. Host/build/ICG gates and the exact I2C1 400 kHz HANDSHAKE/post-read state passed. Outbound reserve/write remains Phase 3 scope.
3. **Manager boundary:** separate legacy frame decode/encode from Flash/session execution; return a response instead of writing the I2C buffer directly.
4. **Logical addressing:** change manager operations from absolute Flash addresses to image offsets and inject Flash operations/configuration.
5. **Platform handover:** move VTOR/MSP/IRQ/SysTick/watchdog/LED/power preparation into the HC32 platform port.
6. **Second transport proof:** add UART and prove that I2C and UART share the same parser and manager. Do not implement USB/CAN/ETH/BLE until one is required.
7. **Image lifecycle:** activate metadata A/B, complete-image integrity, version/rollback policy and power-loss recovery.
8. **Second MCU proof:** port the BSP/platform layer to one different MCU and keep protocol/manager source unchanged.

## Verification gates

- Host: parser/codec vectors, manager state transitions, range/overflow/alignment, retries, abort, version policy and power-loss state-machine simulation.
- Target: Flash erase/program/readback, linker/map bounds, VTOR/MSP handover and reset causes.
- HIL: each transport, interrupted update, watchdog timing, power loss during erase/write/metadata commit, ACK-before-jump and rollback.

Phase 1 has passed its host build/test gate, HC32 linker/ICG gate and one exact I2C1 400 kHz HANDSHAKE HIL transaction. Repeated I2C soak, real Flash update, power-loss and application-handover HIL remain future gates rather than blockers for starting phase 2.

Phase 2 passed its software gates and the exact I2C1 400 kHz HANDSHAKE after a fresh reset. A preserved attempt outside the ten-minute update window returned controlled empty bytes because the known timeout busy wait delayed main-loop RX consumption; the in-window retry and post-read ownership/counter capture passed.

## Completion criteria

The abstraction is proven only when HC32 I2C and a second transport share the same core, followed by a second MCU using unchanged protocol/manager sources. Until then, avoid factories, dynamic allocation, transport registries and speculative protocol features.
