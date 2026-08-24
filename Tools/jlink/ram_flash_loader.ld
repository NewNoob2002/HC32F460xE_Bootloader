ENTRY(loader_main)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x20018000, LENGTH = 0x00007000
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text.loader_main))
        *(.text*)
        *(.rodata*)
    } > RAM

    .data :
    {
        *(.data*)
    } > RAM

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    } > RAM
}

ASSERT(loader_main == ORIGIN(RAM), "loader_main must be the first SRAM instruction")
