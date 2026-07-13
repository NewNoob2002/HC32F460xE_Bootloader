#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#define BOOT_ENABLE_LEGACY_PROTOCOL      1
#define BOOT_ENABLE_HANDSHAKE            1
#define BOOT_ENABLE_FLASH_UPDATE         0
#define BOOT_ENABLE_APPLICATION_JUMP     1
#define BOOT_ENABLE_SOFTWARE_RESET_ENTRY 1
#define BOOT_ENABLE_DEBUG_LOG            0
#define BOOT_ENABLE_LED_STATUS           0
#define BOOT_ENABLE_WATCHDOG             0 /* Board watchdog contract is UNKNOWN. */
#define BOOT_ENABLE_HOST_TESTS           0

#define BOOT_I2C_SLAVE_ADDRESS           0x11U
#define BOOT_APPLICATION_DATA_MAX        512U
#define BOOT_UPDATE_WINDOW_POLLS         1000000UL

/* Exact legacy command and ACK values are absent from this checkout. */
#define BOOT_LEGACY_PROFILE_CONFIRMED    0

#endif
