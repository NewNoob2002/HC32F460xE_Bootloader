#include "bsp_clock.h"
#include "system_hc32f460.h"
void bsp_clock_init(void) {
    SystemCoreClockUpdate();
}
