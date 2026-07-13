# Board-foundation build report

## HardFault instrumentation build

After adding the exception snapshot and corrected startup order:

| Variant | Flash | Data | BSS reported by `size` |
| --- | ---: | ---: | ---: |
| Debug | 19604 B | 248 B | 5100 B |
| Release | 16680 B | 248 B | 4332 B |
| ReleaseNoLog | 10176 B | 0 B | 3016 B |

The Debug ELF SHA-256 is
`786ac758cead41255fa15f66af9b7d74ba56cdd60f6d54ca402c369ebd8253e3`.
All three builds and the host tests pass.

## Recovery I2C/handshake baseline

| Variant | Flash | Main RAM used | Free Boot Flash |
| --- | ---: | ---: | ---: |
| Release | 16440 B | 4400 B | 16328 B |
| Debug | 18208 B | 5168 B | 14560 B |
| ReleaseNoLog | 9940 B | 2836 B | 22828 B |

The transport reserves two 544-byte RX buffers and one 544-byte TX buffer. The
streaming Parser occupies 548 bytes including state/alignment. Debug uses a
1024-byte RTT up-buffer; Release retains 256 bytes.

All variants generate ELF, HEX, BIN, and MAP files. Host CRC, Parser, codec,
and mode tests pass. Symbol inspection confirms that I2C and Protocol modules
are linked and no `EFM_Program`, `EFM_SectorErase`, legacy erase, or legacy
Flash-write function is reachable.

Toolchain: GNU Arm Embedded GCC 14.3.1. Linker: `HC32F460xE.ld`. Application base: `0x00008000`.

| Variant | Flash | Data | Main BSS | Stack | Total static RAM | Change from previous stage |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Release, RTT + EasyLogger | 10784 B | 248 B | 1400 B | 2048 B | 3696 B | +2712 B from 8072 B |
| Debug, RTT + EasyLogger | 12032 B | 248 B | 1400 B | 2048 B | 3696 B | n/a |
| Release, logging disabled | 4536 B | 0 B | 104 B | 2048 B | 2152 B | +1584 B from 2952 B |

Release occupies 32.91% of the 32 KiB Boot region and leaves 21984 bytes free. The generic GNU `size` BSS figure includes 68 bytes of non-RAM OTP allocation; main-BSS and total-static-RAM values above come from the linker map.

Commands:

```sh
cmake --preset gcc-release --fresh
cmake --build --preset gcc-release
cmake --preset Debug --fresh
cmake --build --preset Debug
cmake --preset ReleaseNoLog --fresh
cmake --build --preset ReleaseNoLog
cmake --preset HostTests --fresh
cmake --build --preset HostTests
ctest --preset HostTests
```

Release SHA-256:

- ELF: `eeecdeb015478d80019350fc961fe42e4134ae52d9e6626249a25f2496a6d68d`
- HEX: `98c7f31bb3ef2ae3e19ac3e5af68df42eefa44c36d02e749e22cea6c419d35f9`
- BIN: `2ecc4034824a819970e3e80e3d44f4c916b68d86eb02831a527f91921cab3a3c`
- MAP: `6ccb52138da1f80b58a3d4b5d4d0c9cba26b28daccc1242a5b3a7b46b7e99bbc`

Disassembly confirms PB3 initialization is called before the PSPCR function; PSPCR uses `strh` with `0x0003`, PA6/PB5 initialization calls `GPIO_ResetPins`, `GPIO_Init`, and latch readback, and protocol/I2C and Flash erase/program symbols are absent from the linked image.
