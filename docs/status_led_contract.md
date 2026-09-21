# Status-LED contract

Current implementation status: [current_status.md](current_status.md).

PB5 drives the `NET_STATE` 2N7002 gate as a CMOS push-pull output. The external 4.7 kOhm pull-down and firmware preload keep it off initially. PB5 high turns the LED on; PB5 low turns it off.

LED initialization happens only after application validation and mode selection, in Recovery or the update window. A normal boot with a valid application leaves PB5 uninitialized by Boot. Booting is continuously on. Update mode toggles every 250 ms, recovery every 1000 ms, and fatal mode every 100 ms. Mode changes restart timing deterministically. During upgrade handover the LED remains active until the jump response has completed and the handover guard has elapsed; application handover then turns an initialized LED off.
