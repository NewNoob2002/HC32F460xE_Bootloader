# Hardware smoke test

Current implementation status: [current_status.md](current_status.md). No current-revision hardware update test has been executed; CI and host mocks are not substitutes for these checks.

## Blank-application recovery handshake

With only Boot programmed, verify PB3 high, PA6 DONE pulses, and the PB5
one-second Recovery pattern. RTT must report application invalid, Recovery, and
I2C1 ready at address `0x11`.

Send the 21-byte request below as a write ending in STOP, wait 5–20 ms, then
perform a separate 21-byte read:

`AA 44 18 01 FE 0C 00 20 11 00 00 00 00 00 00 00 00 00 00 D4 E3`

Expected response:

`AA 44 18 01 FE 0C 00 20 00 00 00 00 00 00 00 00 00 00 00 A0 6E`

Repeat 100 times and confirm no reset, stale response, PB3 transition, or
watchdog interruption. Physical execution remains pending.

1. Probe PB3, MCU reset, and the Linux power rail through at least 100 software resets. Require no power-disabling low pulse or Linux reboot.
2. Verify PB3 GPIO operation, PA13/PA14 SWD, RTT visibility, reset reconnect, and connect-under-reset after PSPCR `0x0003`.
3. Probe PA6: idle low, approximately 1 ms high pulses every approximately 3000 ms, no stuck-high state, and no late-poll burst. Run longer than 60 seconds and require TPL5010 RSTn inactive.
4. Verify PB5 high turns `NET_STATE` on, low turns it off, all patterns match, and it is low before application entry.
5. At handover verify PB3 high, PA6 low, PB5 low, then confirm the application starts and resumes DONE feeding before timeout.
6. Program a disposable test application, run HANDSHAKE/ERASE/DOWNLOAD/JUMP, verify every ACK and compare the programmed image before allowing handover.
7. Repeat with invalid address, unaligned address, truncated frame, CRC error, duplicate erase, missing erase, early jump and an unread prior ACK.
8. Interrupt power separately during erase, program and any future metadata commit. Recovery must never jump to a partial image.
