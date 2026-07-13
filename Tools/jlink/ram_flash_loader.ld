ENTRY(loader_main)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x20018000, LENGTH = 0x00007000
}

SECTIONS
{
    .text :
    {
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
