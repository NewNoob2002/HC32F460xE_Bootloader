#include "legacy_protocol.h"
#include "bsp_i2c_slave.h"
#include "legacy_parser.h"

static legacy_parser_t parser;
void legacy_protocol_init(void) { legacy_parser_reset(&parser); }
void legacy_protocol_poll(void)
{
    uint8_t input[BSP_I2C_RX_CAPACITY];
    size_t length = 0U;
    if (bsp_i2c_slave_take_rx(input, sizeof(input), &length)) {
        (void)legacy_parser_push(&parser, input, length, true);
        /* Dispatch is disabled until legacy command/ACK/offset evidence is supplied. */
        legacy_parser_reset(&parser);
    }
}
