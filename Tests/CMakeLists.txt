function(add_boot_protocol_test target mode flash_update erase download jump simulation)
    add_executable(${target}
        boot_protocol_parser_tests.c
        boot_protocol_crc_tests.c
        ../App/Src/boot_update_service.c
    )
    target_include_directories(${target} PRIVATE
        ../Protocol/Inc ../Core/Inc ../App/Inc ../Drivers/BSP/Inc
    )
    target_compile_definitions(${target} PRIVATE
        BOOT_CFG_LOGGING=0 BOOT_HOST_TEST=1 BOOT_TEST_UPDATE_MODE=${mode}
        BOOT_ENABLE_FLASH_UPDATE=${flash_update}
        BOOT_ENABLE_LEGACY_ERASE=${erase}
        BOOT_ENABLE_LEGACY_DOWNLOAD=${download}
        BOOT_ENABLE_LEGACY_JUMP=${jump}
        BOOT_ENABLE_LEGACY_UPDATE_SIMULATION=${simulation}
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
    target_link_libraries(${target} PRIVATE boot::protocol)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_boot_protocol_test(boot_protocol_tests 1 1 1 1 1 0)
add_boot_protocol_test(boot_protocol_disabled_tests 0 0 0 0 0 0)
add_boot_protocol_test(boot_protocol_simulation_tests 2 1 1 1 1 1)

add_executable(boot_core_tests
    boot_core_tests.c
    ../Core/Src/boot_memory_map.c
    ../App/Src/app_validator.c
    ../App/Src/boot_app.c
)
target_include_directories(boot_core_tests PRIVATE
    ../Core/Inc ../App/Inc
)
target_compile_definitions(boot_core_tests PRIVATE BOOT_CFG_LOGGING=0 BOOT_HOST_TEST=1)
target_compile_options(boot_core_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
target_link_libraries(boot_core_tests PRIVATE project::options boot::config)
add_test(NAME boot_core_tests COMMAND boot_core_tests)
