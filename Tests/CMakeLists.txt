add_library(boot_test_config INTERFACE)
target_include_directories(boot_test_config INTERFACE ../Boot/Inc ../Drivers/BSP/Inc)
target_compile_definitions(boot_test_config INTERFACE BOOT_CFG_LOGGING=0 FIRMWARE_CFG_LOGGING=0)
target_link_libraries(boot_test_config INTERFACE project::options firmware::config)

function(add_boot_protocol_test target mode flash_update erase download jump simulation)
    add_executable(${target}
        boot_protocol_parser_tests.c
        boot_protocol_crc_tests.c
        ../Boot/Src/boot_update_service.c
    )
    target_compile_definitions(${target} PRIVATE
        BOOT_TEST_UPDATE_MODE=${mode}
        BOOT_ENABLE_FLASH_UPDATE=${flash_update}
        BOOT_ENABLE_LEGACY_ERASE=${erase}
        BOOT_ENABLE_LEGACY_DOWNLOAD=${download}
        BOOT_ENABLE_LEGACY_JUMP=${jump}
        BOOT_ENABLE_LEGACY_UPDATE_SIMULATION=${simulation}
    )
    target_link_libraries(${target} PRIVATE firmware::protocol boot_test_config)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_boot_protocol_test(boot_protocol_tests 1 1 1 1 1 0)
add_boot_protocol_test(boot_protocol_disabled_tests 0 0 0 0 0 0)
add_boot_protocol_test(boot_protocol_simulation_tests 2 1 1 1 1 1)

add_executable(boot_core_tests
    boot_core_tests.c
    ../Boot/Src/boot_memory_map.c
    ../Boot/Src/app_validator.c
    ../Boot/Src/boot_app.c
    ../Boot/Src/boot_status_led.c
)
target_link_libraries(boot_core_tests PRIVATE boot_test_config)
add_test(NAME boot_core_tests COMMAND boot_core_tests)

add_executable(boot_handover_tests
    boot_handover_tests.c
    ../Drivers/BSP/Src/bsp_i2c_slave.c
    ../Drivers/BSP/Src/bsp_reset.c
    ../Boot/Src/boot_reset.c
)
target_include_directories(boot_handover_tests PRIVATE hc32_mock)
target_link_libraries(boot_handover_tests PRIVATE boot_test_config)
add_test(NAME boot_handover_tests COMMAND boot_handover_tests)

# 每个产品单独编译共用代码，故意不提供 Boot/Inc 搜索路径。
function(add_product_test product oled lvgl)
    set(target shared_${product}_tests)
    add_executable(${target} shared_firmware_tests.c ../Drivers/BSP/Src/bsp_status_led.c)
    target_include_directories(${target} PRIVATE hc32_mock ../Drivers/BSP/Inc)
    target_compile_definitions(${target} PRIVATE
        EXPECTED_PRODUCT_NAME="${product}" EXPECTED_HAS_OLED=${oled} EXPECTED_USE_LVGL=${lvgl}
    )
    target_link_libraries(${target} PRIVATE app::${product} project::options)
    add_test(NAME ${target} COMMAND ${target})
endfunction()
add_product_test(X1 0 0)
add_product_test(X1Pro 1 1)
add_product_test(XBase 1 1)

find_program(HC32_ARM_GCC arm-none-eabi-gcc)
if(HC32_ARM_GCC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME linker_layout_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/linker_layout_tests.py ${HC32_ARM_GCC})
else()
    message(STATUS "GNU Arm toolchain unavailable: linker layout regression runs in firmware CI")
endif()
