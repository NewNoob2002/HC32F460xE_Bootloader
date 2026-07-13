#ifndef BOOT_LOG_H
#define BOOT_LOG_H
#include "boot_config.h"
#if BOOT_ENABLE_EASYLOGGER
#include "elog.h"
#define BOOT_LOG_INFO(tag, ...)  log_i((tag), __VA_ARGS__)
#define BOOT_LOG_DEBUG(tag, ...) log_d((tag), __VA_ARGS__)
#define BOOT_LOG_ERROR(tag, ...) log_e((tag), __VA_ARGS__)
#else
#define BOOT_LOG_INFO(tag, ...)  ((void)0)
#define BOOT_LOG_DEBUG(tag, ...) ((void)0)
#define BOOT_LOG_ERROR(tag, ...) ((void)0)
#endif
#endif
