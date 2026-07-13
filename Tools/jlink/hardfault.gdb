set pagination off
set confirm off
target remote localhost:2331
monitor reset
monitor halt
break Reset_Handler
break SystemInit
break main
break bsp_power_init
break bsp_i2c_slave_init
break i2c_eei_callback
break i2c_rxi_callback
break i2c_tei_callback
break HardFault_Handler
continue
# After HardFault_Handler is reached, use:
# p/x g_boot_fault_snapshot
# x/8wx g_boot_fault_snapshot.msp
# x/6i g_boot_fault_snapshot.stacked_pc-6
# info line *g_boot_fault_snapshot.stacked_pc
# info symbol g_boot_fault_snapshot.stacked_lr
