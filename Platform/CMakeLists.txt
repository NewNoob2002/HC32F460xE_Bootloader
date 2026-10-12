# 通过 INTERFACE sources 编译到镜像中，确保 startup 不被静态库抽取规则丢弃。
add_library(hc32_platform_headers INTERFACE)
add_library(hc32::headers ALIAS hc32_platform_headers)
set(HC32_ROOT ${PROJECT_SOURCE_DIR})
target_include_directories(hc32_platform_headers INTERFACE
    ${HC32_ROOT}/Drivers/BSP/Inc
    ${HC32_ROOT}/Config
    ${HC32_ROOT}/Drivers/CMSIS/Include
    ${HC32_ROOT}/Drivers/CMSIS/Device/HDSC/hc32f4xx/Include
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/inc
)
target_compile_definitions(hc32_platform_headers INTERFACE HC32F460 USE_DDL_DRIVER)
target_compile_options(hc32_platform_headers INTERFACE
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og;-g3;-fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-Os;-g0>
)
target_link_libraries(hc32_platform_headers INTERFACE project::options firmware::config)

add_library(hc32_platform INTERFACE)
add_library(hc32::platform ALIAS hc32_platform)
target_sources(hc32_platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/HC32F460/Src/syscalls.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_timebase.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_clock.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_gpio.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_reset.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_write_protection.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_power.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_power_policy.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_debug_port.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_external_watchdog.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_status_led.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_i2c_slave.c
    ${HC32_ROOT}/Drivers/BSP/Src/bsp_flash.c
    ${HC32_ROOT}/Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/system_hc32f460.c
    ${HC32_ROOT}/Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_rmu.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_gpio.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_clk.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_utility.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_efm.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_pwc.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_sram.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_i2c.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_interrupts.c
    ${HC32_ROOT}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_fcg.c
)
target_link_libraries(hc32_platform INTERFACE hc32::headers)
