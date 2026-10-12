#ifndef BOOT_LOG_H
#define BOOT_LOG_H
#include "boot_config.h"
#ifndef FIRMWARE_CFG_LOGGING
#define FIRMWARE_CFG_LOGGING BOOT_CFG_LOGGING
#endif
#include "firmware_log.h"
#define BOOT_LOG_INFO(...)  FIRMWARE_LOG_INFO(__VA_ARGS__)
#define BOOT_LOG_DEBUG(...) FIRMWARE_LOG_DEBUG(__VA_ARGS__)
#define BOOT_LOG_WARN(...)  FIRMWARE_LOG_WARN(__VA_ARGS__)
#define BOOT_LOG_ERROR(...) FIRMWARE_LOG_ERROR(__VA_ARGS__)
#endif
