#ifndef X1_MOCK_HC32_LL_H
#define X1_MOCK_HC32_LL_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { RESET = 0, SET = 1 } en_flag_status_t;
#define CLK_STB_FLAG_PLL  1U
#define CLK_STB_FLAG_XTAL 2U
#define RMU_FLAG_PWR_ON   1U

/* X1 按键 GPIO：仅模拟驱动实际消费的字段。 */
typedef struct {
    uint16_t u16PinDir, u16PullUp;
} stc_gpio_init_t;
#define GPIO_PORT_A 0U
#define GPIO_PIN_00 0x0001U
#define GPIO_PIN_15 0x8000U
#define GPIO_FUNC_0 0U
#define PIN_DIR_IN  0U
#define PIN_PU_OFF  0U
#define PIN_PU_ON   1U
#define PIN_RESET   0U
#define PIN_SET     1U
#define LL_OK       0
int32_t GPIO_StructInit(stc_gpio_init_t* config);
void GPIO_SetFunc(uint8_t port, uint16_t pins, uint8_t function);
int32_t GPIO_Init(uint8_t port, uint16_t pin, const stc_gpio_init_t* config);
uint16_t GPIO_ReadInputPins(uint8_t port, uint16_t pin);
extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);
en_flag_status_t CLK_GetStableStatus(uint8_t flag);

typedef uint32_t bsp_irq_state_t;
bsp_irq_state_t bsp_enter_critical(void);
void bsp_exit_critical(bsp_irq_state_t state);

#ifdef __cplusplus
}
#endif
#endif
