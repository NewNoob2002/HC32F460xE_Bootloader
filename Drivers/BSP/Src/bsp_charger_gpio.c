#include "bsp_charger_gpio.h"
#include "hc32_ll.h"

static bool ready;
bool bsp_charger_gpio_init(void) {
    stc_gpio_init_t config;
    ready = false;
    (void)GPIO_StructInit(&config);
    config.u16PinDir = PIN_DIR_OUT;
    config.u16PinState = PIN_STAT_RST;
    // V1.3旧HAL：PA4高使能，PB10高选择3A；识别前不得配置这些输出。
    GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_04);
    GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_10);
    if ((GPIO_Init(GPIO_PORT_A, GPIO_PIN_04, &config) != LL_OK)
        || (GPIO_Init(GPIO_PORT_B, GPIO_PIN_10, &config) != LL_OK))
        return false;
    config.u16PinDir = PIN_DIR_IN;
    config.u16PullUp = PIN_PU_ON;
    if ((GPIO_Init(GPIO_PORT_A, GPIO_PIN_01, &config) != LL_OK)
        || (GPIO_Init(GPIO_PORT_B, GPIO_PIN_06 | GPIO_PIN_07, &config) != LL_OK))
        return false;
    ready = true;
    return true;
}
bool bsp_charger_gpio_typec_present(void) {
    return ready && (GPIO_ReadInputPins(GPIO_PORT_A, GPIO_PIN_01) == PIN_SET);
}
bool bsp_charger_gpio_detect(void) {
    return ready && (GPIO_ReadInputPins(GPIO_PORT_B, GPIO_PIN_07) == PIN_SET);
}
void bsp_charger_gpio_enable(bool enabled) {
    if (!ready)
        return;
    if (enabled) {
        GPIO_SetPins(GPIO_PORT_B, GPIO_PIN_10);
        GPIO_SetPins(GPIO_PORT_A, GPIO_PIN_04);
    } else {
        GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_04);
        GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_10);
    }
}
