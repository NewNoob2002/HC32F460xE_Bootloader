#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_board_config.h"
#include "bsp_status_led.h"
#include "bsp_timebase.h"
#include "firmware_layout.h"
#include "hc32_ll.h"

#ifdef BOOT_CONFIG_H
#error "Shared drivers and product profiles must not depend on Boot configuration"
#endif

static bool gpio_level;
static bool gpio_read_mismatch;
static bool gpio_init_fail;
static unsigned gpio_writes;

static void check_led_pin(uint8_t port, uint16_t pin) {
    assert(port == BSP_STATUS_LED_PORT && pin == BSP_STATUS_LED_PIN);
}
int32_t GPIO_StructInit(stc_gpio_init_t* init) {
    memset(init, 0, sizeof(*init));
    return LL_OK;
}
int32_t GPIO_Init(uint8_t port, uint16_t pin, const stc_gpio_init_t* init) {
    check_led_pin(port, pin);
    assert(init->u16PinDir == PIN_DIR_OUT);
    assert(init->u16PinOutputType == PIN_OUT_TYPE_CMOS);
    assert(init->u16PinState == PIN_STAT_RST);
    if (gpio_init_fail)
        return -1;
    gpio_level = false;
    return LL_OK;
}
uint16_t GPIO_ReadOutputPins(uint8_t port, uint16_t pin) {
    check_led_pin(port, pin);
    return (gpio_read_mismatch || gpio_level) ? PIN_SET : PIN_RESET;
}
void GPIO_SetPins(uint8_t port, uint16_t pin) {
    check_led_pin(port, pin);
    gpio_level = true;
    ++gpio_writes;
}
void GPIO_ResetPins(uint8_t port, uint16_t pin) {
    check_led_pin(port, pin);
    gpio_level = false;
    ++gpio_writes;
}

int main(void) {
    assert(strcmp(APP_PRODUCT_NAME, EXPECTED_PRODUCT_NAME) == 0);
    assert(APP_HAS_OLED == EXPECTED_HAS_OLED);
    assert(APP_USE_LVGL == EXPECTED_USE_LVGL);
    assert(BOOT_FLASH_BASE == 0U && BOOT_FLASH_SIZE == 32UL * 1024UL);
    assert(APP_FLASH_BASE == BOOT_FLASH_BASE + BOOT_FLASH_SIZE);
    assert(APP_FLASH_END == BOOT_METADATA_A_BASE);
    assert(APP_PARAM_SECTOR_BASE == 0x0007E000UL && APP_PARAM_SECTOR_SIZE == 8UL * 1024UL);
    assert(APP_PARAM_SECTOR_END == PHYSICAL_FLASH_END_EXCLUSIVE);
    assert(!bsp_time_elapsed(99U, 0U, 100U));
    assert(bsp_time_elapsed(100U, 0U, 100U));
    assert(!bsp_time_elapsed(3U, UINT32_MAX - 5U, 10U));
    assert(bsp_time_elapsed(4U, UINT32_MAX - 5U, 10U));

    bsp_status_led_write(true);
    bsp_status_led_fault_on();
    assert(gpio_writes == 0U && !bsp_status_led_is_ready());
    assert(bsp_status_led_init());
    assert(bsp_status_led_is_ready() && !gpio_level);
    bsp_status_led_write(true);
    assert(gpio_level == (BSP_STATUS_LED_ACTIVE_LEVEL != 0U));
    bsp_status_led_write(false);
    assert(gpio_level == (BSP_STATUS_LED_INACTIVE_LEVEL != 0U));
    bsp_status_led_fault_on();
    assert(gpio_level == (BSP_STATUS_LED_ACTIVE_LEVEL != 0U));

    gpio_init_fail = true;
    assert(!bsp_status_led_init() && !bsp_status_led_is_ready());
    const unsigned writes_before = gpio_writes;
    bsp_status_led_write(false);
    assert(gpio_writes == writes_before);
    gpio_init_fail = false;
    gpio_read_mismatch = true;
    assert(!bsp_status_led_init() && !bsp_status_led_is_ready());
    bsp_status_led_fault_on();
    assert(gpio_writes == writes_before);
    gpio_read_mismatch = false;
    assert(bsp_status_led_init() && !gpio_level);
    printf("shared_firmware_tests: %s PASS\n", APP_PRODUCT_NAME);
    return 0;
}
