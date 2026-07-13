#include "bsp_debug_port.h"
#include "bsp_board_config.h"
void bsp_debug_port_release_watchdog_pin(void) {
    WRITE_REG16(CM_GPIO->PSPCR, BSP_DEBUG_PSPCR_VALUE);
    __DSB();
    __ISB();
}
