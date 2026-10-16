#ifndef BSP_POWER_H
#define BSP_POWER_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool bsp_power_init(void);
void bsp_power_hold_assert(void);
void bsp_power_hold_deassert(void);
bool bsp_power_hold_is_asserted(void);
#ifdef __cplusplus
}
#endif
#endif
