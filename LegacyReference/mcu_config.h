#pragma once
#include <stdint.h>

#define EXAMPLE_PERIPH_WE                                                      \
  (LL_PERIPH_GPIO | LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU |    \
   LL_PERIPH_SRAM)
#define EXAMPLE_PERIPH_WP (LL_PERIPH_FCG | LL_PERIPH_SRAM)

#define XTAL_PORT                   (GPIO_PORT_H)
#define XTAL_PIN                    (GPIO_PIN_00 | GPIO_PIN_01)

/*POWER*/
#define POWER_CONTROL_PIN		PB3
#define WATCHDOG_FEED_PIN		PA6
#define WATCHDOG_FEED_TIME	3000

/*Status LED*/
#define POWER_LED_PIN PC13
#define CHARGE_LED_PIN PH2
#define FUNCTION_LED_PIN PB5

typedef struct ledState_t {
    uint32_t lastToggleTime;
    uint16_t currentRate;
} ledState_t;