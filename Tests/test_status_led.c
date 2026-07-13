#include "bsp_status_led.h"
#include <assert.h>
typedef struct { bool level; unsigned writes; } led_mock_t;
static void write_led(bool level, void *context) { led_mock_t *mock = context; mock->level = level; ++mock->writes; }
void test_status_led(void)
{
    led_scheduler_t led;
    led_mock_t mock = {false, 0U};
    led_scheduler_init(&led, true, write_led, &mock);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_BOOTING, 10U); assert(mock.level);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_UPDATE_WINDOW, 20U); assert(!mock.level);
    led_scheduler_poll(&led, 269U); assert(!mock.level);
    led_scheduler_poll(&led, 270U); assert(mock.level);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_RECOVERY, 300U);
    led_scheduler_poll(&led, 1300U); assert(mock.level);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_FATAL, 1400U);
    led_scheduler_poll(&led, 1500U); assert(mock.level);
    led_scheduler_init(&led, false, write_led, &mock);
    led_scheduler_set_mode(&led, BOOT_LED_MODE_BOOTING, 0U); assert(!mock.level);
    assert(mock.writes >= 8U);
}
