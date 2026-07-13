add_executable(boot_host_tests
    test_main.c
    ../Core/Src/boot_memory_map.c
    ../App/Src/app_validator.c
    ../Protocol/Src/legacy_crc16.c
    ../Protocol/Src/legacy_parser.c
)
target_include_directories(boot_host_tests PRIVATE ../Core/Inc ../App/Inc ../Protocol/Inc)
target_compile_definitions(boot_host_tests PRIVATE BOOT_HOST_TEST=1)
target_compile_options(boot_host_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
add_test(NAME boot_host_tests COMMAND boot_host_tests)
