set pagination off
set confirm off
set print pretty on
target remote :2331
monitor halt
set {unsigned int}0x1fff9428 = 0

echo === SystemInit entry ===\n
hbreak SystemInit
monitor reset
continue
info registers pc lr sp xpsr primask
x/wx 0xE000ED88
x/wx 0xE000ED08

echo === CPACR write and barriers ===\n
advance *0x19de
x/wx 0xE000ED88
stepi
x/wx 0xE000ED88
stepi
info registers pc
stepi
info registers pc

echo === VTOR write and barriers ===\n
advance *0x19ec
x/wx 0xE000ED08
stepi
x/wx 0xE000ED08
stepi
stepi
info registers pc primask
delete breakpoints

echo === main entry ===\n
hbreak main
continue
info registers pc lr sp xpsr primask
x/wx 0xE000ED88
x/wx 0xE000ED08
delete breakpoints

echo === GPIO_ReadOutputPins calls ===\n
hbreak GPIO_ReadOutputPins
continue
info registers r0 r1 lr pc
bt
continue
info registers r0 r1 lr pc
bt
continue
info registers r0 r1 lr pc
bt
delete breakpoints

echo === I2C initialization entry ===\n
hbreak bsp_i2c_slave_init
hbreak HardFault_Handler
continue
info registers pc lr sp xpsr primask
x/wx 0xE000ED88
x/wx 0xE000ED08
delete breakpoints

echo === first I2C VFP instruction ===\n
hbreak *0x2354
hbreak HardFault_Handler
continue
info registers pc lr sp xpsr primask
x/wx 0xE000ED88
stepi
info registers pc
x/wx 0xE000ED88
delete breakpoints

echo === Recovery I2C poll reached ===\n
hbreak bsp_i2c_slave_poll
hbreak HardFault_Handler
continue
info registers pc lr sp xpsr primask
bt
x/wx 0xE000ED28
x/wx 0xE000ED2C
x/wx 0xE000ED88
p/x g_boot_fault_snapshot.magic
detach
quit
