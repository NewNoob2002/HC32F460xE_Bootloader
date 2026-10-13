function(add_app_product product)
    include(${CMAKE_CURRENT_SOURCE_DIR}/Products/${product}/product.cmake)
    if(APP_USE_LVGL AND NOT APP_HAS_OLED)
        message(FATAL_ERROR "${product}: LVGL requires OLED in these products")
    endif()
    add_library(app_${product} INTERFACE)
    add_library(app::${product} ALIAS app_${product})
    target_compile_definitions(app_${product} INTERFACE
        APP_PRODUCT_NAME="${product}"
        APP_HAS_OLED=$<BOOL:${APP_HAS_OLED}>
        APP_USE_LVGL=$<BOOL:${APP_USE_LVGL}>
    )
    target_link_libraries(app_${product} INTERFACE firmware::config)
    set_property(TARGET app_${product} PROPERTY INTERFACE_HC32_APP_PRODUCT ${product})
    set_property(TARGET app_${product} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING HC32_APP_PRODUCT)
endfunction()

# 产品配置与可执行镜像独立，X1Pro/XBase 暂不创建入口。
add_app_product(X1)
add_app_product(X1Pro)
add_app_product(XBase)

if(NOT BOOT_BUILD_HOST_TESTS)
    # 镜像链接配置与产品配置分开，主机测试不继承 ARM 链接选项。
    add_library(hc32_app_image INTERFACE)
    add_library(app::image ALIAS hc32_app_image)
    set(APP_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Linker/HC32F460_app.ld)
    set(HC32_LINKER_DIR ${PROJECT_SOURCE_DIR}/Platform/HC32F460/Linker)
    target_link_libraries(hc32_app_image INTERFACE hc32::platform)
    target_compile_definitions(hc32_app_image INTERFACE VECT_TAB_OFFSET=0x00008000U)
    target_link_options(hc32_app_image INTERFACE
        ${CPU_FLAGS} -Wl,-L,${HC32_LINKER_DIR} -T${APP_LINKER_SCRIPT}
        -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs
    )
    set_property(TARGET hc32_app_image PROPERTY INTERFACE_LINK_DEPENDS
        "${APP_LINKER_SCRIPT};${HC32_LINKER_DIR}/HC32F460_sections.ld")

    set(APP_TARGET hc32f460_app_x1)
    add_executable(${APP_TARGET}
        Products/X1/main.cpp
        Shared/Src/app_runtime.cpp
        Shared/Src/app_panel.cpp
        Shared/Src/app_battery.cpp
        Shared/Src/app_battery_compensation.cpp
        Shared/Src/app_param_store.cpp
        Shared/Src/app_recording.cpp
        Shared/Src/app_power.cpp
        Shared/Src/app_keys.cpp
        ${PROJECT_SOURCE_DIR}/Libraries/MultiButton/multi_button.c
        Shared/Protocol/Src/panel_protocol.cpp
        ${PROJECT_SOURCE_DIR}/Drivers/BSP/Src/bsp_i2c_master.c
        ${PROJECT_SOURCE_DIR}/Drivers/BSP/Src/bsp_charger_gpio.c
        ${PROJECT_SOURCE_DIR}/Drivers/BSP/Src/bsp_keys.c
        ${PROJECT_SOURCE_DIR}/Drivers/BSP/Src/bsp_indicator_led.c
        Shared/Src/hc32f460_it.cpp
    )
    target_include_directories(${APP_TARGET} PRIVATE Shared/Inc Shared/Protocol/Inc
        ${PROJECT_SOURCE_DIR}/Libraries/MultiButton)
    target_compile_features(${APP_TARGET} PRIVATE cxx_std_17)
    target_compile_options(${APP_TARGET} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>
    )
    target_compile_definitions(${APP_TARGET} PRIVATE
        FIRMWARE_CFG_LOGGING=$<BOOL:${APP_ENABLE_LOGGING}>
        APP_BUILD_ID="x1-p3"
        $<$<CONFIG:Debug>:BUFFER_SIZE_UP=8192>
    )
    target_link_libraries(${APP_TARGET} PRIVATE app::X1 app::image firmware::battery_devices)
    if(APP_ENABLE_LOGGING)
        target_link_libraries(${APP_TARGET} PRIVATE firmware::logging)
    endif()
    # INTERFACE 源码的属性须在镜像 target 所在目录设置。
    set_source_files_properties(
        ${PROJECT_SOURCE_DIR}/Libraries/easylogger/src/elog.c
        ${PROJECT_SOURCE_DIR}/Libraries/easylogger/src/elog_utils.c
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion"
    )
    set_source_files_properties(${PROJECT_SOURCE_DIR}/Libraries/MultiButton/multi_button.c
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
    set_source_files_properties(${PROJECT_SOURCE_DIR}/Drivers/hc32f4xx_ll_drivers/src/hc32_ll_pwc.c
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
    target_link_options(${APP_TARGET} PRIVATE
        -Wl,-Map=${PROJECT_BINARY_DIR}/${APP_TARGET}.map -Wl,--print-memory-usage
    )
    set_target_properties(${APP_TARGET} PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        SUFFIX .elf RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    add_custom_command(TARGET ${APP_TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${APP_TARGET}> ${PROJECT_BINARY_DIR}/${APP_TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${APP_TARGET}> ${PROJECT_BINARY_DIR}/${APP_TARGET}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${APP_TARGET}>
        COMMENT "Generating X1 App HEX/BIN and size report"
    )
endif()
