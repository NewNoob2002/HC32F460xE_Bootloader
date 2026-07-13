#include "bsp_power.h"
#include "bsp_board_config.h"
static bool power_gpio_configured;
void bsp_power_hold_assert(void) {
    GPIO_SetPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN);
}
bool bsp_power_init(void) {
    stc_gpio_init_t init;
    GPIO_SetPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN);
    (void)GPIO_StructInit(&init);
    init.u16PinState = PIN_STAT_SET;
    init.u16PinDir = PIN_DIR_OUT;
    init.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    power_gpio_configured = GPIO_Init(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN, &init) == LL_OK;
    if (power_gpio_configured)
        bsp_power_hold_assert();
    return power_gpio_configured;
}
bool bsp_power_hold_is_asserted(void) {
    return power_gpio_configured && (GPIO_ReadOutputPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN) == PIN_SET);
}
