#pragma once
#include "liteParse.h"
#include "flash.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t txBuffer_temp[256];

int message_decode(Parser16 *parse, uint8_t *txBuffer);

#ifdef __cplusplus
}
#endif