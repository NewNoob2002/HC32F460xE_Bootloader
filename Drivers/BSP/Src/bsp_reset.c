#include "bsp_reset.h"
#include "hc32_ll_rmu.h"
uint32_t bsp_reset_capture_and_clear(void) {
    uint32_t flags = 0U;
    if (RMU_GetStatus(RMU_FLAG_SW) == SET)
        flags |= RMU_FLAG_SW;
    RMU_ClearStatus();
    return flags;
}
bool bsp_reset_was_software(uint32_t flags) {
    return (flags & RMU_FLAG_SW) != 0U;
}
