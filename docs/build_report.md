# Board-foundation build report

Toolchain: GNU Arm Embedded GCC 14.3.1. Linker: `HC32F460xE.ld`. Application base: `0x00008000`.

| Variant | Flash | Data | BSS | Stack | Total static RAM | Delta from 2132-byte baseline |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Release, RTT + EasyLogger | 8072 B | 248 B | 1224 B | 2048 B | 3520 B | +5940 B |
| Debug, RTT + EasyLogger | 8848 B | 248 B | 1224 B | 2048 B | 3520 B | +6716 B |
| Release, logging disabled | 2952 B | 0 B | 104 B | 2048 B | 2152 B | +820 B |

The generic `size` BSS number also counts 68 bytes of non-RAM OTP allocation; the table uses main-SRAM map values. Release retains 24696 bytes within the 32 KiB Boot region. RTT/EasyLogger attribution is detailed in `logging_architecture.md`.

Commands:

```sh
cmake --preset Debug --fresh && cmake --build --preset Debug
cmake --preset Release --fresh && cmake --build --preset Release
cmake --preset ReleaseNoLog --fresh && cmake --build --preset ReleaseNoLog
cmake --preset HostTests --fresh && cmake --build --preset HostTests
ctest --preset HostTests
```

Release SHA-256:

- ELF: `022ab9e2ec274dcdf0faa9580a713b6a40f16d003d658c74cb6d12c7e3df905c`
- HEX: `3eafb5f03a247e528594e0859edcdadc7cbc363b44514ea459c458b26fb20816`
- BIN: `6533b9e3a36f76871356e0244265edaee24bec15e3e30c696600751c93281a0d`
- MAP: `961dad27f5a3cb444f0880249c0123b44ea08bbbb90716e867c1ed1ec219b13b`
