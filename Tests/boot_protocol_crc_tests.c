#include "boot_protocol_crc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Runs deterministic legacy CRC compatibility vectors. */
void boot_protocol_crc_tests_run(void) {
    static const uint8_t payload[] = {0x20U, 0x11U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

    assert(BootProtocolCrcCalculate(NULL, 0U) == 0U);
    assert(BootProtocolCrcCalculate((const uint8_t*)"123456789", 9U) == 0x31C3U);
    assert(BootProtocolCrcCalculate(payload, sizeof(payload)) == 0xE3D4U);
}
