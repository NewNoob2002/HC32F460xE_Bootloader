#ifndef LEGACY_PROTOCOL_H
#define LEGACY_PROTOCOL_H
#include <stdint.h>
void legacy_protocol_service_init(uint32_t now_ms);
void legacy_protocol_service_poll(uint32_t now_ms);
#endif
