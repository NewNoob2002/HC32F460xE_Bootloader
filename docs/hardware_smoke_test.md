# Hardware smoke test

No hardware tests have been executed yet.

1. Probe PB3, MCU reset, and the Linux power rail through at least 100 software resets. Require no power-disabling low pulse or Linux reboot.
2. Verify PB3 GPIO operation, PA13/PA14 SWD, RTT visibility, reset reconnect, and connect-under-reset after PSPCR `0x0003`.
3. Probe PA6: idle low, approximately 1 ms high pulses every approximately 3000 ms, no stuck-high state, and no late-poll burst. Run longer than 60 seconds and require TPL5010 RSTn inactive.
4. Verify PB5 high turns `NET_STATE` on, low turns it off, all patterns match, and it is low before application entry.
5. At handover verify PB3 high, PA6 low, PB5 low, then confirm the application starts and resumes DONE feeding before timeout.

