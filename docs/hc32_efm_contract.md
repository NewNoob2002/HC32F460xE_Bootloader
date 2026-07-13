# HC32 EFM contract

The contract is recovered from the checked-in HC32 DDL:
`Drivers/hc32f4xx_ll_drivers/inc/hc32_ll_efm.h` and
`Drivers/hc32f4xx_ll_drivers/src/hc32_ll_efm.c`.

- Physical Flash addresses are `0x00000000..0x0007FFFF` (`EFM_END_ADDR`).
- Sector size is `EFM_SECTOR_SIZE == 0x2000` and erase takes a byte address via
  `EFM_SectorErase(uint32_t u32Addr)`. The address must be word aligned.
- `EFM_Program()` accepts a byte length but asserts word-aligned destination;
  it writes complete words and pads a final partial word with `0xFF`.
- `EFM_ProgramWord()` and `EFM_ProgramWordReadBack()` require word alignment.
  Boot V2 therefore uses a 4-byte program unit and aligned staging words.
- The DDL clears EFM status, saves/disables cache, changes operation mode,
  waits for completion with bounded timeouts, restores read-only mode, and
  restores cache state.
- `EFM_SequenceProgram()` and chip erase are marked `__RAM_FUNC`; the ordinary
  single-program and sector-erase functions are not. The DDL implementation is
  the authority for this distinction.
- `EFM_ClearStatus(EFM_FLAG_ALL)` and `EFM_WaitEnd()` provide status/error and
  timeout handling inside the DDL.

No repository application linker script or production application map is
present, so the highest used application address cannot yet be verified against
`0x0007A000`. Destructive OTA remains gated off until that evidence is added.

