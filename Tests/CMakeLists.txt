add_executable(boot_protocol_tests
    boot_protocol_parser_tests.c
    boot_protocol_crc_tests.c
    ../App/Src/boot_update_service.c
)
target_include_directories(boot_protocol_tests PRIVATE
    ../Protocol/Inc ../Core/Inc ../App/Inc ../Drivers/BSP/Inc
)
target_compile_definitions(boot_protocol_tests PRIVATE BOOT_CFG_LOGGING=0)
target_compile_options(boot_protocol_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
target_link_libraries(boot_protocol_tests PRIVATE boot::protocol)
add_test(NAME boot_protocol_tests COMMAND boot_protocol_tests)
