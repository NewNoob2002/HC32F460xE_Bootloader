# Minimal bootloader refactor plan

1. Preserve vendor CMSIS, DDL, startup assembly, and root linker script; replace the broken application-oriented target with a minimal boot target at `0x00000000` limited to 32 KiB.
2. Add Core, App, Protocol, and BSP layers without moving vendor files.
3. Centralize the confirmed memory map and feature options. Keep unknown metadata and pins unused.
4. Implement and host-test CRC, bounded streaming parser, and vector validation before hardware integration.
5. Add safe jump and reset-source wrappers using existing CMSIS/DDL definitions only.
6. Add an interrupt-safe I2C transport core. Keep board pin setup explicitly unavailable until the legacy pin map is supplied.
7. Build ELF/BIN/HEX/MAP, record sizes/checksums, and document exact reproducible commands.
8. Document manual flash and smoke tests; do not claim physical results without hardware evidence.

Stage B success in this checkout means a clean, size-bounded firmware build and passing host tests. Exact legacy handshake bytes and operational I2C pins remain blocked by absent source evidence and will be clearly reported rather than guessed.
