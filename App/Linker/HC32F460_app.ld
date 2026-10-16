/* HC32F460xE App：X1/X1Pro/XBase 共用地址布局，不携带 Boot 的 ICG。 */
FLASH_ORIGIN = DEFINED(FLASH_ORIGIN) ? FLASH_ORIGIN : 0x00008000;
FLASH_LENGTH = DEFINED(FLASH_LENGTH) ? FLASH_LENGTH : 0x00072000;
MEMORY
{
    FLASH       (rx): ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_LENGTH
    OTP_DATA    (rx): ORIGIN = 0x03000C00, LENGTH = 960
    OTP_LOCK    (rx): ORIGIN = 0x03000FC0, LENGTH = 60
    RAM        (xrw): ORIGIN = 0x1FFF8000, LENGTH = 188K
    RET_RAM    (xrw): ORIGIN = 0x200F0000, LENGTH = 4K
}
SECTIONS
{
    ASSERT(FLASH_ORIGIN == 0x00008000, "app image must start at 0x8000")
    ASSERT(FLASH_LENGTH == 0x00072000, "app image region must end at 0x7A000")
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
        . = ALIGN(4);
    } >FLASH
    ASSERT(ADDR(.vectors) == FLASH_ORIGIN && SIZEOF(.vectors) >= 8,
           "app vector table must start at 0x8000")

    /* 明确收集并拒绝误加入 App 的 ICG，防止成为未受检查的孤立段。 */
    .icg_sec :
    {
        KEEP(*(.icg_sec))
    } >FLASH
    ASSERT(SIZEOF(.icg_sec) == 0, "ICG belongs to Boot and must not be linked into App")
}
INCLUDE HC32F460_sections.ld
