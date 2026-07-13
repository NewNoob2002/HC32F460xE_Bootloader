#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#define BOOT_ENABLE_LEGACY_PROTOCOL      0
#define BOOT_ENABLE_HANDSHAKE            0
#define BOOT_ENABLE_FLASH_UPDATE         0
#define BOOT_ENABLE_APPLICATION_JUMP     1
#define BOOT_ENABLE_SOFTWARE_RESET_ENTRY 1
#ifndef BOOT_CFG_LOGGING
#define BOOT_CFG_LOGGING                 1
#endif
#define BOOT_ENABLE_SEGGER_RTT           BOOT_CFG_LOGGING
#define BOOT_ENABLE_EASYLOGGER           BOOT_CFG_LOGGING
#define BOOT_ENABLE_DEBUG_LOG            BOOT_CFG_LOGGING
#define BOOT_ENABLE_STATUS_LED           1
#define BOOT_ENABLE_POWER_HOLD           1
#define BOOT_ENABLE_EXTERNAL_WATCHDOG    1
#define BOOT_ENABLE_HOST_TESTS           0

#define BOOT_I2C_SLAVE_ADDRESS           0x11U
#define BOOT_APPLICATION_DATA_MAX        512U
#define BOOT_UPDATE_WINDOW_MS            10000UL

#ifndef BOOT_BUILD_ID
#define BOOT_BUILD_ID "unknown"
#endif

/* Exact legacy command and ACK values are absent from this checkout. */
#define BOOT_LEGACY_PROFILE_CONFIRMED    0

#endif
