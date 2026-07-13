add_executable(boot_host_tests
    test_main.c
    ../Protocol/Src/legacy_crc16.c
    ../Protocol/Src/legacy_parser.c
    ../Protocol/Src/legacy_codec.c
    ../App/Src/boot_app.c
)
target_include_directories(boot_host_tests PRIVATE ../Protocol/Inc ../Core/Inc ../App/Inc)
target_compile_definitions(boot_host_tests PRIVATE BOOT_CFG_LOGGING=0)
target_compile_options(boot_host_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
add_test(NAME boot_host_tests COMMAND boot_host_tests)
