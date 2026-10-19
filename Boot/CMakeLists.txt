set(BOOT_TARGET hc32f460_boot)
set(BOOT_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Linker/HC32F460_boot.ld)
set(HC32_LINKER_DIR ${PROJECT_SOURCE_DIR}/Platform/HC32F460/Linker)

add_executable(${BOOT_TARGET}
    Src/main.c
    Src/boot_state.c
    Src/boot_memory_map.c
    Src/hc32f460_it.c
    Src/boot_app.c
    Src/boot_update_service.c
    Src/app_validator.c
    Src/app_jump.c
    Src/boot_reset.c
    Src/boot_status_led.c
    # 固定 0x400 的 ICG 由 Boot 拥有，App 复用平台时不能再携带该块。
    ${PROJECT_SOURCE_DIR}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_icg.c
)
# INTERFACE sources 的源文件属性须设置在创建镜像 target 的目录作用域。
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/Libraries/easylogger/src/elog.c
    ${PROJECT_SOURCE_DIR}/Libraries/easylogger/src/elog_utils.c
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion"
)
set_source_files_properties(${PROJECT_SOURCE_DIR}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_pwc.c
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
target_include_directories(${BOOT_TARGET} PRIVATE Inc)
target_compile_definitions(${BOOT_TARGET} PRIVATE
    VECT_TAB_OFFSET=0U
    BOOT_CFG_LOGGING=$<BOOL:${BOOT_ENABLE_LOGGING}>
    FIRMWARE_CFG_LOGGING=$<BOOL:${BOOT_ENABLE_LOGGING}>
    BOOT_BUILD_ID="814873c-board"
    $<$<CONFIG:Debug>:BOOT_LOG_DEBUG_BUILD=1>
    $<$<CONFIG:Debug>:BUFFER_SIZE_UP=8192>
)
target_link_libraries(${BOOT_TARGET} PRIVATE hc32::platform firmware::protocol)
if(BOOT_ENABLE_LOGGING)
    target_link_libraries(${BOOT_TARGET} PRIVATE firmware::logging)
endif()
target_link_options(${BOOT_TARGET} PRIVATE
    ${CPU_FLAGS} -Wl,-L,${HC32_LINKER_DIR} -T${BOOT_LINKER_SCRIPT}
    -Wl,--defsym=FLASH_ORIGIN=0x00000000
    -Wl,--defsym=FLASH_LENGTH=0x00008000
    -Wl,-Map=${PROJECT_BINARY_DIR}/${BOOT_TARGET}.map
    -Wl,--gc-sections -Wl,--print-memory-usage
    --specs=nano.specs --specs=nosys.specs
)
# 保持原有工具、CI 与调试配置使用的产物路径。
set_target_properties(${BOOT_TARGET} PROPERTIES
    SUFFIX .elf RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    LINK_DEPENDS "${BOOT_LINKER_SCRIPT};${HC32_LINKER_DIR}/HC32F460_sections.ld"
)
add_custom_command(TARGET ${BOOT_TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${BOOT_TARGET}> ${PROJECT_BINARY_DIR}/${BOOT_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${BOOT_TARGET}> ${PROJECT_BINARY_DIR}/${BOOT_TARGET}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${BOOT_TARGET}>
    COMMENT "Generating HEX/BIN and size report"
)
