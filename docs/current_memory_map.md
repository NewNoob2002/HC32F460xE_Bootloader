# Current memory map

| Region | Start | End exclusive | Size | Evidence |
| --- | ---: | ---: | ---: | --- |
| Boot reservation | `0x00000000` | `0x00008000` | 32 KiB | confirmed application origin |
| Application | `0x00008000` | `0x00080000` | 480 KiB | CMake origin plus device Flash size |
| Main SRAM | `0x1FFF8000` | `0x20027000` | 188 KiB | linker script |
| Retention SRAM | `0x200F0000` | `0x200F1000` | 4 KiB | linker script |
| OTP data | `0x03000C00` | `0x03000FC0` | 960 B | linker script |
| OTP lock | `0x03000FC0` | `0x03000FFC` | 60 B | linker script |

The current root linker script is internally unsafe for its application override: origin is changed to `0x8000` but length remains 512 KiB. The refactor will use explicit boot origin/length and central compile-time assertions. Metadata/flag placement is `UNKNOWN` and will not be accessed.

