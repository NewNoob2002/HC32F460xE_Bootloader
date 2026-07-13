#include "bsp_power.h"
#include "bsp_board_config.h"
#include "bsp_power_policy.h"
static bool power_initialized;
void bsp_power_hold_assert(void) {
    GPIO_SetPins(BSP_POWER_PORT, BSP_POWER_PIN);
    power_initialized = GPIO_ReadOutputPins(BSP_POWER_PORT, BSP_POWER_PIN) == PIN_SET;
}
void bsp_power_init() {
    stc_gpio_init_t init;
    GPIO_SetPins(BSP_POWER_PORT, BSP_POWER_PIN);
    (void)GPIO_StructInit(&init);
    init.u16PinState = PIN_STAT_SET;
    init.u16PinDir = PIN_DIR_OUT;
    init.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    if (GPIO_Init(BSP_POWER_PORT, BSP_POWER_PIN, &init) == LL_OK)
        bsp_power_hold_assert();
}
bool bsp_power_hold_is_asserted(void) {
    return power_initialized && (GPIO_ReadOutputPins(BSP_POWER_PORT, BSP_POWER_PIN) == PIN_SET);
}
