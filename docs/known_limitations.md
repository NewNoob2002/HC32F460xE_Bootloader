# Known limitations

- Destructive ERASE/DOWNLOAD/JUMP commands are reachable even though their feature macros are zero; the gates are not wired to command dispatch.
- Image acceptance checks only MSP and reset vectors. Whole-image length, CRC/hash, signature, version, anti-rollback and persistent VALID/UPDATING metadata are absent.
- Parser partial-frame timeout is implemented but not called by `main()`.
- I2C TX publication has no busy or bounds guard and can overwrite an unread response.
- Handover does not explicitly relocate VTOR or complete the documented watchdog/LED/power preparation.
- The ten-minute timeout also applies in Recovery and repeats if deasserting PB3 does not remove power.
- PB3 glitches, PA6 waveform, PB5 polarity, SWD retention, RTT visibility and real Flash update remain unverified on current hardware.
- Debugger halts can prevent polling long enough for the external TPL5010 to reset the MCU.
- Application base remains `0x00008000`; Metadata A/B are reserved but unused.
