#ifndef BOOT_LOG_H
#define BOOT_LOG_H
#include "boot_config.h"
#if BOOT_ENABLE_EASYLOGGER
#include "elog.h"
#define BOOT_LOG_INFO(...)  log_i(__VA_ARGS__)
#define BOOT_LOG_DEBUG(...) log_d(__VA_ARGS__)
#define BOOT_LOG_WARN(...)  log_w(__VA_ARGS__)
#define BOOT_LOG_ERROR(...) log_e(__VA_ARGS__)
#else
#define BOOT_LOG_INFO(...)  ((void)0)
#define BOOT_LOG_DEBUG(...) ((void)0)
#define BOOT_LOG_WARN(...)  ((void)0)
#define BOOT_LOG_ERROR(...) ((void)0)
#endif
#endif
