cmake_minimum_required(VERSION 3.20)
if(BOOT_BUILD_HOST_TESTS)
    project(HC32F460_Bootloader LANGUAGES C)
else()
    project(HC32F460_Bootloader LANGUAGES C ASM)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BOOT_BUILD_HOST_TESTS "Build hardware-independent host tests" OFF)
option(BOOT_ENABLE_LOGGING "Enable SEGGER RTT and EasyLogger" ON)

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

add_library(boot_project_options INTERFACE)
add_library(project::options ALIAS boot_project_options)
target_compile_options(boot_project_options INTERFACE -Wall -Wextra -Wshadow -Wconversion -Wundef)
if(NOT BOOT_BUILD_HOST_TESTS)
    target_compile_options(boot_project_options INTERFACE ${CPU_FLAGS})
endif()

add_library(boot_config_target INTERFACE)
add_library(boot::config ALIAS boot_config_target)
target_include_directories(boot_config_target INTERFACE Config Core/Inc)

add_library(boot_protocol STATIC
    Protocol/Src/boot_protocol_parser.c
    Protocol/Src/boot_protocol_crc.c
)
add_library(boot::protocol ALIAS boot_protocol)
target_include_directories(boot_protocol PUBLIC Protocol/Inc PRIVATE
    Drivers/BSP/Inc
    Drivers/CMSIS/Include)
target_link_libraries(boot_protocol PUBLIC project::options boot::config)

if(BOOT_BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(Tests)
    return()
endif()

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
    App/Src/boot_update_service.c
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
    Drivers/BSP/Src/bsp_i2c_slave.c
    Drivers/BSP/Src/bsp_flash.c
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
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_i2c.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_interrupts.c
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_fcg.c
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
    $<$<CONFIG:Debug>:BUFFER_SIZE_UP=8192>
)
target_compile_options(${BOOT_TARGET} PRIVATE
    ${CPU_FLAGS} -ffunction-sections -fdata-sections
    -Wall -Wextra -Wshadow -Wconversion -Wundef
    $<$<CONFIG:Debug>:-Og;-g3;-fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-Os;-g0>
)
target_link_libraries(${BOOT_TARGET} PRIVATE boot::protocol)
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
