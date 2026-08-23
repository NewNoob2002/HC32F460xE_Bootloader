# Cross-platform bootloader plan

Status: approved design plan; implementation has not started. The current HC32 behavior remains the baseline while boundaries are extracted incrementally.

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

Use one compile-time-selected transport first. It exposes non-blocking `read`, `write`, `poll` and `tx_idle`. UART/USB/TCP can pass byte chunks directly; CAN or BLE adapters own their required fragmentation/reassembly. Add multi-transport registration only when simultaneous transports are required.

### Protocol adapter

Each wire protocol owns framing, CRC and response encoding, and emits transport-neutral commands: `QUERY`, `BEGIN`, `WRITE`, `FINISH`, `ABORT` and `ACTIVATE`. Upgrade commands carry an image offset rather than an MCU absolute address. The current legacy adapter converts its absolute address field to an offset after range validation.

### Upgrade manager

Own session state, range/order rules, erase/write progress, whole-image verification, version policy, abort and activation. It must not include MCU headers or call transport/LED functions.

### Platform port

Provide only operations the manager actually needs: Flash erase/write/read, time, watchdog service, reset cause, handover preparation and jump. Memory base, capacity, erase size and write alignment are target configuration, not protocol constants.

## Migration phases

1. **Safety baseline:** make destructive feature gates control reachable dispatch; fix or explicitly document response ownership, parser timeout and handover behavior.
2. **Transport boundary:** move I2C polling out of `boot_protocol_parser.c`; feed byte chunks through the existing parser API.
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

## Completion criteria

The abstraction is proven only when HC32 I2C and a second transport share the same core, followed by a second MCU using unchanged protocol/manager sources. Until then, avoid factories, dynamic allocation, transport registries and speculative protocol features.
