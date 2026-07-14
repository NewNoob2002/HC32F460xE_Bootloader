#ifndef BOOT_PROTOCOL_CRC_H
#define BOOT_PROTOCOL_CRC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Calculates the legacy augmented CRC-16 for protocol payload bytes.
 * @param data Payload bytes, or NULL when length is zero.
 * @param length Number of payload bytes.
 * @return CRC using polynomial 0x1021, initial zero, and two appended zero bytes.
 */
uint16_t BootProtocolCrcCalculate(const uint8_t* data, size_t length);

#endif
