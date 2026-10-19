# Boot/App 按需链接日志；屏幕、LVGL 和充电芯片库由 App 接入时独立选择。
add_library(firmware_logging INTERFACE)
add_library(firmware::logging ALIAS firmware_logging)
target_sources(firmware_logging INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/SEGGER/SEGGER_RTT.c
    ${CMAKE_CURRENT_SOURCE_DIR}/easylogger/src/elog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/easylogger/src/elog_utils.c
    ${PROJECT_SOURCE_DIR}/Drivers/BSP/Src/bsp_log_port.c
)
target_include_directories(firmware_logging INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/SEGGER
    ${CMAKE_CURRENT_SOURCE_DIR}/easylogger/inc
)
target_link_libraries(firmware_logging INTERFACE hc32::headers)

# X1 按运行时探测选充电方案；芯片驱动不拥有产品状态，不再依赖 Arduino/Wire。
add_library(firmware_battery_devices INTERFACE)
add_library(firmware::battery_devices ALIAS firmware_battery_devices)
target_sources(firmware_battery_devices INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Bq40z50/bq40z50.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mp2762a/mp2762a.cpp
)
target_include_directories(firmware_battery_devices INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Bq40z50 ${CMAKE_CURRENT_SOURCE_DIR}/Mp2762a
)
target_link_libraries(firmware_battery_devices INTERFACE hc32::headers)
