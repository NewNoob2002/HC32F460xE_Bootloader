# Boot V2 memory map

Current implementation status: [current_status.md](current_status.md).

Boot V2 reserves the final three 8 KiB sectors of physical Flash:

| Region | Range | Size |
|---|---:|---:|
| Boot | `0x00000000..0x00007FFF` | 32 KiB |
| Application | `0x00008000..0x00079FFF` | 456 KiB |
| Metadata A | `0x0007A000..0x0007BFFF` | 8 KiB |
| Metadata B | `0x0007C000..0x0007DFFF` | 8 KiB |
| App parameters (X1 SOC compensation) | `0x0007E000..0x0007FFFF` | 8 KiB |

These boundaries are centralized in `Common/Inc/firmware_layout.h` and compile-time checked for sector alignment and a fixed 32 KiB Boot. `Boot/Inc/boot_memory_map.h` contains Boot's range/validation API. The image scripts are `Boot/Linker/HC32F460_boot.ld` (32 KiB) and `App/Linker/HC32F460_app.ld` (456 KiB). They share `Platform/HC32F460/Linker/HC32F460_sections.ld` while enforcing independent Flash boundaries and ICG ownership. App linker smoke tests pass; a production application map is still required to establish its real resource use.

Metadata A/B are reserved but unused. The final sector is assigned to App parameters as `APP_PARAM_SECTOR_*` (2026-10-12, X1 P5); Boot erase/download stay limited to the Application region, and host tests reject Boot erase/download outside it. No App code writes that sector yet. The reachable erase/download/jump feature gates are documented in `Boot/Inc/boot_config.h`; this structure change keeps their existing behavior.
