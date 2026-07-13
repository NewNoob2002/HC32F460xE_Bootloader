set pagination off
set confirm off
set print pretty on
target remote :2331
monitor halt
echo === target/ELF comparison ===\n
compare-sections
echo === live core registers ===\n
info registers r0 r1 r2 r3 r7 sp msp psp lr pc xpsr control primask basepri faultmask
echo === live system registers ===\n
x/wx 0xE000ED08
x/wx 0xE000ED28
x/wx 0xE000ED2C
x/wx 0xE000ED34
x/wx 0xE000ED38
x/wx 0xE000ED88
echo === persistent fault snapshot ===\n
p/x g_boot_fault_snapshot
echo === recovery loop breakpoint ===\n
hbreak bsp_i2c_slave_poll
continue
bt
info registers r0 r1 r2 r3 r7 sp lr pc xpsr primask
x/wx 0xE000ED88
delete breakpoints
echo === GPIO argument capture ===\n
hbreak GPIO_ReadOutputPins
continue
info registers r0 r1 lr pc
bt
detach
quit
