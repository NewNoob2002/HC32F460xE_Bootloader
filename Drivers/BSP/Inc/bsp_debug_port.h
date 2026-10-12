#ifndef BSP_DEBUG_PORT_H
#define BSP_DEBUG_PORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void bsp_debug_port_configure_for_gpio(void);
uint16_t bsp_debug_port_pspcr(void);
#ifdef __cplusplus
}
#endif
#endif
