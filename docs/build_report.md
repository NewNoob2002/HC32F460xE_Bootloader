# Baseline build report

- Source revision before changes: `814873c`
- Compiler: GNU Arm Embedded GCC 14.3.1
- Configuration: `gcc-release`
- Linker: root `HC32F460xE.ld`, boot region `0x00000000 + 0x8000`
- Result: success, zero compiler/linker warnings in the final build
- Loadable Flash: 2,132 bytes (6.51% of 32 KiB)
- Initialized data: 0 bytes
- BSS: 1,104 bytes
- Reserved stack: 2,048 bytes
- Heap: 0 bytes
- Total static RAM reservation: 3,152 bytes
- Host tests: 1/1 CTest executable passed; CRC, parser bounds/fragment/reset, and vector validation checks passed
- Artifacts and SHA-256: `docs/build_metadata.json`

The map shows Flash exactly bounded to `0x00000000..0x00007FFF`, main SRAM at `0x1FFF8000` for 188 KiB, and retention SRAM at `0x200F0000` for 4 KiB. No section overlap or region overflow was reported.
