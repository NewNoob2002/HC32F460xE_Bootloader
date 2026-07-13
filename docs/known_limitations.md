# Known limitations

- PB3 glitch-free behavior, PA6 waveform, PB5 polarity, SWD retention, and RTT visibility are implemented from confirmed contracts but not physically measured.
- Debugger halts can suspend the polling scheduler long enough for the external TPL5010 to reset the MCU.
- The application must take over DONE feeding promptly after handover.
- Protocol/I2C processing, Flash erase/programming, OTA metadata, and the full updater remain disabled.
- Application base remains `0x00008000`.

