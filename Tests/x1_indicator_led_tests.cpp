#include <assert.h>
#include <stdio.h>
#include "bsp_indicator_led.h"
#include "hc32_ll.h"

namespace {
constexpr uint8_t kPorts[] = {GPIO_PORT_C, GPIO_PORT_H};
constexpr uint16_t kPins[] = {GPIO_PIN_13, GPIO_PIN_02};
bool level[2];
bool configured[2];
unsigned init_calls;
unsigned fail_at; // 第 N 次 GPIO_Init 失败，0 表示不失败。
unsigned writes;

unsigned index_of(uint8_t port, uint16_t pin) {
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((port == kPorts[index]) && (pin == kPins[index]))
            return index;
    }
    assert(false && "unexpected indicator GPIO");
    return 0U;
}

void reset(unsigned failure) {
    level[0] = level[1] = true; // 初始化必须显式拉低。
    configured[0] = configured[1] = false;
    init_calls = writes = 0U;
    fail_at = failure;
}
} // namespace

extern "C" {
int32_t GPIO_StructInit(stc_gpio_init_t* config) {
    *config = {};
    config->u16PinDir = PIN_DIR_IN;
    config->u16PinState = 1U; // 与 PIN_STAT_RST 不同，强制驱动显式设置。
    return LL_OK;
}
int32_t GPIO_Init(uint8_t port, uint16_t pin, const stc_gpio_init_t* config) {
    const unsigned index = index_of(port, pin);
    assert(config->u16PinDir == PIN_DIR_OUT && config->u16PinState == PIN_STAT_RST);
    assert(config->u16PinOutputType == PIN_OUT_TYPE_CMOS);
    if (++init_calls == fail_at)
        return -1;
    configured[index] = true;
    level[index] = false;
    return LL_OK;
}
void GPIO_SetPins(uint8_t port, uint16_t pin) {
    const unsigned index = index_of(port, pin);
    assert(configured[index]);
    level[index] = true;
    ++writes;
}
void GPIO_ResetPins(uint8_t port, uint16_t pin) {
    const unsigned index = index_of(port, pin);
    assert(configured[index]);
    level[index] = false;
    ++writes;
}
} // extern "C"

int main() {
    // 任一引脚初始化失败时保持未就绪，写入不得触碰 GPIO。
    for (unsigned failure = 1U; failure <= 2U; ++failure) {
        reset(failure);
        assert(!bsp_indicator_led_init());
        bsp_indicator_led_write(BSP_INDICATOR_LED_POWER, true);
        bsp_indicator_led_write(BSP_INDICATOR_LED_CHARGE, true);
        assert(writes == 0U);
    }

    reset(0U);
    assert(bsp_indicator_led_init());
    assert(configured[0] && configured[1] && !level[0] && !level[1]);
    bsp_indicator_led_write(BSP_INDICATOR_LED_POWER, true);
    assert(level[0] && !level[1]);
    bsp_indicator_led_write(BSP_INDICATOR_LED_CHARGE, true);
    bsp_indicator_led_write(BSP_INDICATOR_LED_POWER, false);
    assert(!level[0] && level[1]);
    const unsigned before = writes;
    bsp_indicator_led_write(BSP_INDICATOR_LED_COUNT, true);
    assert(writes == before);
    puts("x1_indicator_led_tests: init failure / active-high outputs / bounds PASS");
}
