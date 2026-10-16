#include "bsp_status_led.h"
#include "bsp_board_config.h"
#include "hc32_ll.h"

static volatile bool board_ready;

void bsp_status_led_write(bool on) {
    if (!board_ready)
        return;
    if (on == (BSP_STATUS_LED_ACTIVE_LEVEL != 0U))
        GPIO_SetPins(BSP_STATUS_LED_PORT, BSP_STATUS_LED_PIN);
    else
        GPIO_ResetPins(BSP_STATUS_LED_PORT, BSP_STATUS_LED_PIN);
}

bool bsp_status_led_init(void) {
    stc_gpio_init_t init;
    board_ready = false;
    (void)GPIO_StructInit(&init);
    init.u16PinState = PIN_STAT_RST;
    init.u16PinDir = PIN_DIR_OUT;
    init.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    if (GPIO_Init(BSP_STATUS_LED_PORT, BSP_STATUS_LED_PIN, &init) != LL_OK)
        return false;
    if (GPIO_ReadOutputPins(BSP_STATUS_LED_PORT, BSP_STATUS_LED_PIN) != PIN_RESET)
        return false;
    board_ready = true;
    bsp_status_led_write(false);
    return true;
}

bool bsp_status_led_is_ready(void) {
    return board_ready;
}
void bsp_status_led_fault_on(void) {
    bsp_status_led_write(true);
}
