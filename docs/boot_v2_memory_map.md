# Boot V2 memory map

Current implementation status: [current_status.md](current_status.md).

Boot V2 reserves the final three 8 KiB sectors of physical Flash:

| Region | Range | Size |
|---|---:|---:|
| Boot | `0x00000000..0x00007FFF` | 32 KiB |
| Application | `0x00008000..0x00079FFF` | 456 KiB |
| Metadata A | `0x0007A000..0x0007BFFF` | 8 KiB |
| Metadata B | `0x0007C000..0x0007DFFF` | 8 KiB |
| Reserved | `0x0007E000..0x0007FFFF` | 8 KiB |

These boundaries are centralized in `Core/Inc/boot_memory_map.h` and compile-time checked for sector alignment. The current repository contains only the Boot linker script; an application map is still required to prove the application fits below `0x0007A000`.

Metadata A/B and the reserved sector are not used. Real erase/program commands are currently reachable despite the destructive feature macros being zero, so those macros are not an effective gate in this revision.
