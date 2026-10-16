/* HC32F460xE Boot：固定前 32 KiB，独占芯片 ICG 配置。 */
FLASH_ORIGIN = DEFINED(FLASH_ORIGIN) ? FLASH_ORIGIN : 0x00000000;
FLASH_LENGTH = DEFINED(FLASH_LENGTH) ? FLASH_LENGTH : 0x00008000;
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
    ASSERT(FLASH_ORIGIN == 0x00000000, "boot image must start at Flash base")
    ASSERT(FLASH_LENGTH == 0x00008000, "boot image region must be exactly 32 KiB")
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
        . = ALIGN(4);
    } >FLASH
    ASSERT(ADDR(.vectors) == FLASH_ORIGIN && SIZEOF(.vectors) >= 8,
           "boot vector table must start at Flash base")

    .icg_sec 0x00000400 :
    {
        KEEP(*(.icg_sec))
    } >FLASH
    ASSERT(ADDR(.icg_sec) == 0x00000400, "ICG must start at 0x400")
    ASSERT(SIZEOF(.icg_sec) == 0x20, "ICG must contain exactly eight 32-bit words")
}
INCLUDE HC32F460_sections.ld
