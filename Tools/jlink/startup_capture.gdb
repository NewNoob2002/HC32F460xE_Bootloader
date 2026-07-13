set pagination off
set confirm off
set print pretty on
target remote :2331
monitor reset
monitor halt
compare-sections .vectors
compare-sections .text
compare-sections .rodata
break Reset_Handler
break SystemInit
break main
continue
printf "RESET pc=%#x sp=%#x lr=%#x primask=%#x\n", $pc, $sp, $lr, $primask
continue
printf "SYSTEM_INIT_ENTRY pc=%#x cpacr=", $pc
x/wx 0xE000ED88
printf " vtor="
x/wx 0xE000ED08
printf " primask=%#x\n", $primask
x/14i $pc
tbreak *0x000019E2
continue
printf "AFTER_CPACR_WRITE pc=%#x cpacr=", $pc
x/wx 0xE000ED88
stepi
printf "AFTER_DSB pc=%#x cpacr=", $pc
x/wx 0xE000ED88
stepi
printf "AFTER_ISB pc=%#x cpacr=", $pc
x/wx 0xE000ED88
tbreak *0x000019F6
continue
printf "AFTER_VTOR_BARRIERS pc=%#x vtor=", $pc
x/wx 0xE000ED08
printf " primask=%#x\n", $primask
continue
printf "MAIN_ENTRY pc=%#x sp=%#x primask=%#x cpacr=", $pc, $sp, $primask
x/wx 0xE000ED88
printf " vtor="
x/wx 0xE000ED08
detach
quit
