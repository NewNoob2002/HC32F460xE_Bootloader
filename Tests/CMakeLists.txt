add_executable(boot_host_tests
    test_main.c test_boot_time.c test_status_led.c test_external_watchdog.c test_power_policy.c
    ../Core/Src/boot_memory_map.c ../App/Src/app_validator.c
    ../Protocol/Src/legacy_crc16.c ../Protocol/Src/legacy_parser.c
    ../Drivers/BSP/Src/bsp_external_watchdog.c ../Drivers/BSP/Src/bsp_status_led.c
    ../Drivers/BSP/Src/bsp_power_policy.c
)
target_include_directories(boot_host_tests PRIVATE ../Core/Inc ../App/Inc ../Protocol/Inc ../Drivers/BSP/Inc)
target_compile_definitions(boot_host_tests PRIVATE BOOT_HOST_TEST=1 BOOT_CFG_LOGGING=0)
target_compile_options(boot_host_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wundef)
add_test(NAME boot_host_tests COMMAND boot_host_tests)
