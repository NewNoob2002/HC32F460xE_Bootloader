#ifndef FIRMWARE_LOG_H
#define FIRMWARE_LOG_H
#ifndef FIRMWARE_CFG_LOGGING
#define FIRMWARE_CFG_LOGGING 0
#endif
#if FIRMWARE_CFG_LOGGING
#include "elog.h"
#define FIRMWARE_LOG_INFO(...)  log_i(__VA_ARGS__)
#define FIRMWARE_LOG_DEBUG(...) log_d(__VA_ARGS__)
#define FIRMWARE_LOG_WARN(...)  log_w(__VA_ARGS__)
#define FIRMWARE_LOG_ERROR(...) log_e(__VA_ARGS__)
#else
#define FIRMWARE_LOG_INFO(...)  ((void)0)
#define FIRMWARE_LOG_DEBUG(...) ((void)0)
#define FIRMWARE_LOG_WARN(...)  ((void)0)
#define FIRMWARE_LOG_ERROR(...) ((void)0)
#endif
#endif
