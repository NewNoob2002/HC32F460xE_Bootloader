# Current build report

Verified locally on 2026-08-23. Capability and residual-risk context is in [current_status.md](current_status.md).

## Toolchain

- CMake 3.28.3
- Ninja 1.11.1
- Host GCC 13.3.0
- Arm GNU GCC 13.2.1
- Target `HC32F460xE`, Cortex-M4F hard-float
- Linker script `HC32F460xE.ld`, Boot Flash limit 32 KiB

## Results

| Preset | Flash | Flash use | RAM region use | `size` data | `size` bss | Result |
|---|---:|---:|---:|---:|---:|---|
| Debug | 27,172 B | 82.92% | 14,064 B | 248 B | 13,884 B | pass |
| Release | 23,748 B | 72.47% | 6,120 B | 248 B | 5,940 B | pass |
| ReleaseNoLog | 12,556 B | 38.32% | 4,556 B | 0 B | 4,624 B | pass |
| HostTests | n/a | n/a | n/a | n/a | n/a | 2/2 pass |

All firmware presets generated ELF, HEX, BIN and MAP artifacts. ReleaseNoLog is part of the required verification because it catches direct EasyLogger dependencies outside the logging gate.

## Commands

```sh
cmake --preset HostTests --fresh
cmake --build --preset HostTests
ctest --preset HostTests

cmake --preset Debug --fresh
cmake --build --preset Debug
cmake --preset Release --fresh
cmake --build --preset Release
cmake --preset ReleaseNoLog --fresh
cmake --build --preset ReleaseNoLog
```

## SHA-256

| Preset | ELF | BIN |
|---|---|---|
| Debug | `ca65b205077eac943c9d3111dfda8015b1da063f9645a330358a932485627778` | `32f79af16836330e9229c70a1a0d7654a62c2469e8a066b6efca80387402cccf` |
| Release | `143aa38cf53b932ba39cc90f2cb484802cbc0f388713eaa695c705a60502f614` | `c05f318f822bfcb47daa989062068e6182e6e55cc962379309cf0db7eb4d29c2` |
| ReleaseNoLog | `d470fa401c92074bef782c5f25705c72b112ca4cf7918b150f20d0ddfadc4027` | `ea75a41086ced73429d49734fd6eb7631a27ff34ce27f85fb2a9b51dacd17459` |

Hashes identify this local pre-commit build only; CI rebuilds from the pushed revision and publishes its own artifacts.

## Verification boundary

Host tests exercise portable logic and mocked Flash/I2C contracts. Successful cross-compilation verifies compile/link/layout only. No current target programming, runtime, electrical, transport or power-loss test is claimed.
