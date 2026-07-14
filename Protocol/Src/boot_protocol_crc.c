#include "boot_protocol_crc.h"

/**
 * @brief Advances the legacy augmented CRC by one input byte.
 * @param crc_in Current CRC value.
 * @param byte Next input byte.
 * @return Updated CRC value.
 */
static uint16_t crc_update(uint16_t crc_in, uint8_t byte) {
    uint32_t crc = crc_in;
    uint32_t input = (uint32_t)byte | 0x100UL;

    do {
        crc <<= 1U;
        input <<= 1U;
        if ((input & 0x100UL) != 0UL)
            ++crc;
        if ((crc & 0x10000UL) != 0UL)
            crc ^= 0x1021UL;
    } while ((input & 0x10000UL) == 0UL);

    return (uint16_t)(crc & 0xFFFFUL);
}

uint16_t BootProtocolCrcCalculate(const uint8_t* data, size_t length) {
    uint16_t crc = 0U;

    for (size_t index = 0U; index < length; ++index)
        crc = crc_update(crc, data[index]);
    crc = crc_update(crc, 0U);
    return crc_update(crc, 0U);
}
