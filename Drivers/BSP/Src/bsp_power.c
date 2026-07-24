#include "bsp_power.h"
#include "bsp_board_config.h"
#include "hc32_ll.h"

void bsp_power_hold_assert(void) {
    GPIO_SetPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN);
}

void bsp_power_hold_deassert(void) {
    GPIO_ResetPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN);
}
bool bsp_power_init(void) {
    stc_gpio_init_t init;
    (void)GPIO_StructInit(&init);
    init.u16PinState = PIN_STAT_SET;
    init.u16PinDir = PIN_DIR_OUT;
    init.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    GPIO_Init(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN, &init);
    bsp_power_hold_assert();
    return true;
}
bool bsp_power_hold_is_asserted(void) {
    return (GPIO_ReadOutputPins(BOOT_POWER_HOLD_PORT, BOOT_POWER_HOLD_PIN) == PIN_SET);
}
