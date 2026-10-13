#include "bsp_indicator_led.h"
#include "hc32_ll.h"

typedef struct {
    uint8_t port;
    uint16_t pin;
} indicator_pin_t;

// 旧 X1 mcu_config.h/HAL_Led.cpp：PC13 电源灯、PH2 充电灯，高电平点亮；实物电平待实板确认。
static const indicator_pin_t indicator_pins[BSP_INDICATOR_LED_COUNT] = {
    [BSP_INDICATOR_LED_POWER] = {GPIO_PORT_C, GPIO_PIN_13},
    [BSP_INDICATOR_LED_CHARGE] = {GPIO_PORT_H, GPIO_PIN_02},
};
static bool ready;

bool bsp_indicator_led_init(void) {
    stc_gpio_init_t config;
    ready = false;
    (void)GPIO_StructInit(&config);
    config.u16PinState = PIN_STAT_RST;
    config.u16PinDir = PIN_DIR_OUT;
    config.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    for (unsigned index = 0U; index < (unsigned)BSP_INDICATOR_LED_COUNT; ++index) {
        if (GPIO_Init(indicator_pins[index].port, indicator_pins[index].pin, &config) != LL_OK)
            return false;
    }
    ready = true;
    return true;
}

void bsp_indicator_led_write(bsp_indicator_led_t led, bool on) {
    if (!ready || ((unsigned)led >= (unsigned)BSP_INDICATOR_LED_COUNT))
        return;
    if (on)
        GPIO_SetPins(indicator_pins[led].port, indicator_pins[led].pin);
    else
        GPIO_ResetPins(indicator_pins[led].port, indicator_pins[led].pin);
}
