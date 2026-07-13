# Status-LED contract

PB5 drives the `NET_STATE` 2N7002 gate as a CMOS push-pull output. The external 4.7 kOhm pull-down and firmware preload keep it off initially. PB5 high turns the LED on; PB5 low turns it off.

Booting is continuously on. Update mode toggles every 250 ms, recovery every 1000 ms, and fatal mode every 100 ms. Mode changes restart timing deterministically. OFF and application preparation drive PB5 low while leaving it configured as output.

