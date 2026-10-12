# Status-LED contract

Current implementation status: [current_status.md](current_status.md).

The shared `Drivers/BSP` LED driver owns GPIO initialization and logical on/off writes. `Boot/Src/boot_status_led.c` owns the modes and scheduler below; App can reuse `bsp_status_led_write()` without depending on Boot policy.

PB5 drives the `NET_STATE` 2N7002 gate as a CMOS push-pull output. The external 4.7 kOhm pull-down and firmware preload keep it off initially. PB5 high turns the LED on; PB5 low turns it off.

LED initialization normally happens after application validation and mode selection, in Recovery or the update window. A normal boot with a valid application leaves PB5 uninitialized by Boot. The fatal loop also attempts initialization if needed.

| State | LED behavior |
|---|---|
| Boot initialization | Continuously on |
| Recovery waiting | Toggle every 1000 ms |
| Update window / successful erase, download or accepted jump | Toggle every 250 ms |
| Flash erase/write/readback failure or rejected jump | Toggle every 100 ms; communication and retry remain available |
| Fatal loop with a working timebase | Toggle every 100 ms |
| Timebase initialization failure | Continuously on, if GPIO initialization succeeds |
| HardFault / NMI | Best-effort continuously on, only if Boot already initialized the LED |
| Inactivity shutdown pending | Toggle every 100 ms for 1000 ms |
| Power hold released | Off; no further command processing until reset |
| Application handover / upgrade software reset | Turn an initialized LED off after the ACK and handover guard, before reset or direct handover |

Changing modes restarts timing; setting the same mode preserves phase and output. Invalid download parameters or a download before erase leave the LED unchanged. A successful erase/download/jump clears an update-error indication; handshake and malformed traffic do not. Error fast blinking is an indication, not necessarily a terminal state.

Both Recovery and the update window retain the ten-minute power-off policy, measured from entry or the latest successful erase/download/accepted jump. Successful duplicate erase acknowledgements also refresh the timer. Handshake, rejected requests and Flash failures do not extend it. Timeout starts a nonblocking, one-shot shutdown; commands stop being processed as soon as shutdown starts, while LED and watchdog polling continue. After the delay, Boot turns the LED off and releases PB3 exactly once even if external power remains.

LED timing is driven by main-loop polling, not a hardware waveform. Synchronous Flash operations can still pause visible blinking; elapsed periods are skipped rather than replayed. Fault indication does not depend on SysTick and does not initialize peripherals inside HardFault/NMI. Faults before LED initialization or affecting GPIO/CPU execution may have no visible indication.
