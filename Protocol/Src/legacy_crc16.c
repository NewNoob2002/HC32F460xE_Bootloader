#include "legacy_crc16.h"
uint16_t legacy_crc16_xmodem(const uint8_t* data, size_t length) {
    uint16_t crc = 0U;
    for (size_t index = 0U; index < length; ++index) {
        crc ^= (uint16_t)((uint16_t)data[index] << 8U);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}
