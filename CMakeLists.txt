cmake_minimum_required(VERSION 3.20)
project(HC32F460_Bootloader LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BOOT_BUILD_HOST_TESTS "Build hardware-independent host tests" OFF)

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
    Core/Src/hc32f460_it.c
    Core/Src/syscalls.c
    App/Src/boot_app.c
    App/Src/app_validator.c
    App/Src/app_jump.c
    Protocol/Src/legacy_crc16.c
    Protocol/Src/legacy_parser.c
    Protocol/Src/legacy_protocol.c
    Drivers/BSP/Src/bsp_clock.c
    Drivers/BSP/Src/bsp_gpio.c
    Drivers/BSP/Src/bsp_i2c_slave.c
    Drivers/BSP/Src/bsp_reset.c
    Drivers/BSP/Src/bsp_watchdog.c
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/system_hc32f460.c
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Source/GCC/startup_hc32f460.S
    Drivers/hc32f4xx_ll_drivers/src/hc32_ll_rmu.c
)

target_include_directories(${BOOT_TARGET} PRIVATE
    Core/Inc App/Inc Protocol/Inc Drivers/BSP/Inc Config
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/HDSC/hc32f4xx/Include
    Drivers/hc32f4xx_ll_drivers/inc
)
target_compile_definitions(${BOOT_TARGET} PRIVATE HC32F460 USE_DDL_DRIVER VECT_TAB_OFFSET=0U)
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
