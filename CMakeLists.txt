cmake_minimum_required(VERSION 3.20)
project(HC32F460_Bootloader LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BOOT_BUILD_HOST_TESTS "Build hardware-independent host tests" OFF)
option(BOOT_ENABLE_LOGGING "Enable SEGGER RTT and EasyLogger" ON)

if(BOOT_BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(Tests)
    return()
endif()

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
set(BOOT_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/HC32F460xE.ld)
set(BOOT_TARGET hc32f460_boot)

add_executable(${BOOT_TARGET}
    Core/Src/main.c
    Core/Src/boot_state.c
    Core/Src/boot_memory_map.c
    Core/Src/boot_timebase.c
    Core/Src/hc32f460_it.c
    Core/Src/syscalls.c
    App/Src/boot_app.c
    App/Src/app_validator.c
    App/Src/app_jump.c
    Drivers/BSP/Src/bsp_clock.c
    Drivers/BSP/Src/bsp_gpio.c
    Drivers/BSP/Src/bsp_reset.c
    Drivers/BSP/Src/bsp_write_protection.c
    Drivers/BSP/Src/bsp_power.c
    Drivers/BSP/Src/bsp_power_policy.c
    Drivers/BSP/Src/bsp_debug_port.c
    Drivers/BSP/Src/bsp_external_watchdog.c
    Drivers/BSP/Src/bsp_status_led.c
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/system_hc32f460.c
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_rmu.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_gpio.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_clk.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_utility.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_efm.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_pwc.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_sram.c
)

if(BOOT_ENABLE_LOGGING)
    target_sources(${BOOT_TARGET} PRIVATE
        Libraries/SEGGER/SEGGER_RTT.c
        Libraries/easylogger/src/elog.c
        Libraries/easylogger/src/elog_utils.c
        Libraries/easylogger/port/elog_port.c
    )
endif()

set_source_files_properties(
    Libraries/easylogger/src/elog.c Libraries/easylogger/src/elog_utils.c
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion"
)
set_source_files_properties(
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_pwc.c
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion"
)

target_include_directories(${BOOT_TARGET} PRIVATE
    Core/Inc App/Inc Protocol/Inc Drivers/BSP/Inc Config
    Libraries/SEGGER Libraries/easylogger/inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Include
    Drivers/hc32f4xx_ll_drivers/inc
)
target_compile_definitions(${BOOT_TARGET} PRIVATE
    HC32F460 USE_DDL_DRIVER VECT_TAB_OFFSET=0U
    BOOT_CFG_LOGGING=$<BOOL:${BOOT_ENABLE_LOGGING}>
    BOOT_BUILD_ID="814873c-board"
    $<$<CONFIG:Debug>:BOOT_LOG_DEBUG_BUILD=1>
)
target_compile_options(${BOOT_TARGET} PRIVATE
    ${CPU_FLAGS} -ffunction-sections -fdata-sections
    -Wall -Wextra -Wshadow -Wconversion -Wundef
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-Os;-g0>
)
target_link_options(${BOOT_TARGET} PRIVATE
    ${CPU_FLAGS} -T${BOOT_LINKER_SCRIPT}
    -Wl,--defsym=FLASH_ORIGIN=0x00000000
    -Wl,--defsym=FLASH_LENGTH=0x00008000
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${BOOT_TARGET}.map
    -Wl,--gc-sections -Wl,--print-memory-usage
    --specs=nano.specs --specs=nosys.specs
)
set_target_properties(${BOOT_TARGET} PROPERTIES SUFFIX .elf)

add_custom_command(TARGET ${BOOT_TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${BOOT_TARGET}> ${CMAKE_CURRENT_BINARY_DIR}/${BOOT_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${BOOT_TARGET}> ${CMAKE_CURRENT_BINARY_DIR}/${BOOT_TARGET}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${BOOT_TARGET}>
    COMMENT "Generating HEX/BIN and size report"
)
