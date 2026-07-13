#ifndef BSP_POWER_H
#define BSP_POWER_H
#include <stdbool.h>
bool bsp_power_init(void);
void bsp_power_hold_assert(void);
bool bsp_power_hold_is_asserted(void);
#endif
