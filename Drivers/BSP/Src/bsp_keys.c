#include "bsp_keys.h"
#include "hc32_ll.h"

bool bsp_keys_init(void) {
    stc_gpio_init_t config;
    (void)GPIO_StructInit(&config);
    config.u16PinDir = PIN_DIR_IN;
    // PA0 由电源按键电路驱动，高有效，不用内部上拉制造按下状态。
    config.u16PullUp = PIN_PU_OFF;
    GPIO_SetFunc(GPIO_PORT_A, GPIO_PIN_00 | GPIO_PIN_15, GPIO_FUNC_0);
    if (GPIO_Init(GPIO_PORT_A, GPIO_PIN_00, &config) != LL_OK)
        return false;
    config.u16PullUp = PIN_PU_ON;
    return GPIO_Init(GPIO_PORT_A, GPIO_PIN_15, &config) == LL_OK;
}
bool bsp_keys_power_pressed(void) {
    return GPIO_ReadInputPins(GPIO_PORT_A, GPIO_PIN_00) == PIN_SET;
}
bool bsp_keys_function_pressed(void) {
    return GPIO_ReadInputPins(GPIO_PORT_A, GPIO_PIN_15) == PIN_RESET;
}
