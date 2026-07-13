# Build and flash

## Prerequisites

- CMake 3.20 or newer
- Ninja
- `arm-none-eabi-gcc`, `objcopy`, and `size` discoverable through the existing toolchain prefix

## Clean release build

```sh
cmake --preset gcc-release --fresh
cmake --build --preset gcc-release
```

Artifacts are in `build/gcc-release`: `hc32f460_boot.elf`, `.hex`, `.bin`, and `.map`.

## Host tests

```sh
cmake --preset host-tests --fresh
cmake --build --preset host-tests
ctest --preset host-tests
```

## Flash

The repository contains HC32F460 FLM files but no debugger configuration or historical flash command. The exact existing flash command is therefore `UNKNOWN`. Do not substitute an unverified target/probe command. Import `build/gcc-release/hc32f460_boot.elf` or `.hex` into the board's confirmed production/debug tool, select the HC32F460xE 512 KiB algorithm, program from address zero, verify, and reset. Record tool, probe serial, algorithm, command, and verification log during the manual test.

