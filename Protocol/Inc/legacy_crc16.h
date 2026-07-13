#ifndef LEGACY_CRC16_H
#define LEGACY_CRC16_H
#include <stddef.h>
#include <stdint.h>
uint16_t legacy_crc16_xmodem(const uint8_t *data, size_t length);
#endif
