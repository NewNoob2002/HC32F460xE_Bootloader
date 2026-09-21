#ifndef BSP_RESET_H
#define BSP_RESET_H
#include <stdbool.h>
#include <stdint.h>
uint32_t bsp_reset_capture(void);
bool bsp_reset_was_software(uint32_t flags);
/** @brief Records upgrade completion and requests a real software reset. */
void bsp_reset_after_upgrade(void);
/** @brief Consumes the retained token; true only with a software-reset cause. */
bool bsp_reset_take_upgrade_marker(uint32_t reset_flags);
#endif
