add_executable(boot_protocol_tests
    boot_protocol_parser_tests.c
    boot_protocol_crc_tests.c
    ../App/Src/boot_update_service.c
)
target_include_directories(boot_protocol_tests PRIVATE
    ../Protocol/Inc ../Core/Inc ../App/Inc ../Drivers/BSP/Inc
)
target_compile_definitions(boot_protocol_tests PRIVATE BOOT_CFG_LOGGING=0 BOOT_HOST_TEST=1)
target_compile_options(boot_protocol_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
target_link_libraries(boot_protocol_tests PRIVATE boot::protocol)
add_test(NAME boot_protocol_tests COMMAND boot_protocol_tests)

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
