add_library(firmware_config INTERFACE)
add_library(firmware::config ALIAS firmware_config)
target_include_directories(firmware_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Inc)

# 仅帧解析和 CRC；升级命令处理及 Flash 策略属于 Boot。
add_library(boot_protocol STATIC
    Protocol/Src/boot_protocol_parser.c
    Protocol/Src/boot_protocol_crc.c
)
add_library(firmware::protocol ALIAS boot_protocol)
target_include_directories(boot_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Protocol/Inc)
target_link_libraries(boot_protocol PUBLIC project::options)
