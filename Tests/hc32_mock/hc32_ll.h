#ifndef TEST_HC32_LL_H
#define TEST_HC32_LL_H

#include <stdbool.h>
#include <stdint.h>

/* Only the registers and DDL entry points used by the handover tests. */
typedef struct {
    uint32_t SR, CR2;
    uint8_t DTR, DRR;
} CM_I2C_TypeDef;
typedef struct {
    uint16_t RSTF0;
} CM_RMU_TypeDef;
typedef struct {
    uint16_t FPRC;
} CM_PWC_TypeDef;
typedef float float32_t;
typedef unsigned IRQn_Type;
typedef unsigned en_int_src_t;
typedef void (*func_ptr_t)(void);
typedef struct {
    en_int_src_t enIntSrc;
    IRQn_Type enIRQn;
    func_ptr_t pfnCallback;
} stc_irq_signin_config_t;
typedef struct {
    uint32_t u32ClockDiv, u32Baudrate, u32SclTime;
} stc_i2c_init_t;
enum { RESET = 0, SET = 1, DISABLE = 0, ENABLE = 1, LL_OK = 0 };

/* GPIO fake 用于验证共用 LED 驱动，不引入 Boot 头文件。 */
typedef struct {
    uint16_t u16PinState, u16PinDir, u16PinOutputType;
} stc_gpio_init_t;
#define BSP_STATUS_LED_PORT 1U
#define BSP_STATUS_LED_PIN  0x20U
#define PIN_STAT_RST        0U
#define PIN_STAT_SET        1U
#define PIN_DIR_OUT         1U
#define PIN_OUT_TYPE_CMOS   0U
#define PIN_RESET           0U
#define PIN_SET             1U
int32_t GPIO_StructInit(stc_gpio_init_t* init);
int32_t GPIO_Init(uint8_t port, uint16_t pin, const stc_gpio_init_t* init);
uint16_t GPIO_ReadOutputPins(uint8_t port, uint16_t pin);
void GPIO_SetPins(uint8_t port, uint16_t pin);
void GPIO_ResetPins(uint8_t port, uint16_t pin);
extern CM_I2C_TypeDef mock_i2c;
extern CM_RMU_TypeDef mock_rmu;
extern CM_PWC_TypeDef mock_pwc;

#define BSP_I2C_UNIT                (&mock_i2c)
#define BSP_I2C_BAUDRATE            400000U
#define BSP_I2C_SCL_TIME            5U
#define BSP_I2C_EEI_IRQn            5U
#define BSP_I2C_RXI_IRQn            6U
#define BSP_I2C_TEI_IRQn            4U
#define BSP_I2C_EEI_SOURCE          423U
#define BSP_I2C_RXI_SOURCE          420U
#define BSP_I2C_TEI_SOURCE          422U
#define BSP_I2C_EEI_PRIORITY        9U
#define BSP_I2C_RXI_PRIORITY        10U
#define BSP_I2C_TEI_PRIORITY        10U
#define I2C_CLK_DIV2                1U
#define I2C_ADDR0                   0U
#define I2C_ADDR_7BIT               0U
#define I2C_FLAG_MATCH_ADDR0        0x00002U
#define I2C_FLAG_TX_CPLT            0x00008U
#define I2C_FLAG_STOP               0x00010U
#define I2C_FLAG_ARBITRATE_FAIL     0x00200U
#define I2C_FLAG_NACKF              0x01000U
#define I2C_FLAG_BUSY               0x20000U
#define I2C_FLAG_TRA                0x40000U
#define I2C_CLR_STARTFCLR           0x00001U
#define I2C_CLR_SLADDR0FCLR         I2C_FLAG_MATCH_ADDR0
#define I2C_CLR_STOPFCLR            I2C_FLAG_STOP
#define I2C_CLR_NACKFCLR            I2C_FLAG_NACKF
#define I2C_FLAG_CLR_ARBITRATE_FAIL I2C_FLAG_ARBITRATE_FAIL
#define I2C_FLAG_CLR_TX_CPLT        I2C_FLAG_TX_CPLT
#define I2C_INT_TX_CPLT             I2C_FLAG_TX_CPLT
#define I2C_INT_STOP                I2C_FLAG_STOP
#define I2C_INT_NACK                I2C_FLAG_NACKF
#define I2C_INT_ARBITRATE_FAIL      I2C_FLAG_ARBITRATE_FAIL
#define I2C_INT_MATCH_ADDR0         I2C_FLAG_MATCH_ADDR0
#define I2C_INT_RX_FULL             0x40U

#define FCG_Fcg1PeriphClockCmd(...) ((void)0)
#define GPIO_SetFunc(...)           ((void)0)
static inline void NVIC_ClearPendingIRQ(IRQn_Type irq) {
    (void)irq;
}
static inline void NVIC_EnableIRQ(IRQn_Type irq) {
    (void)irq;
}
static inline void NVIC_DisableIRQ(IRQn_Type irq) {
    (void)irq;
}
static inline void NVIC_SetPriority(IRQn_Type irq, uint32_t priority) {
    (void)irq;
    (void)priority;
}
int32_t INTC_IrqSignIn(const stc_irq_signin_config_t* config);
int32_t INTC_IrqSignOut(IRQn_Type irq);
int32_t I2C_DeInit(CM_I2C_TypeDef* unit);
int32_t I2C_StructInit(stc_i2c_init_t* config);
int32_t I2C_Init(CM_I2C_TypeDef* unit, const stc_i2c_init_t* config, float32_t* error);
void I2C_SlaveAddrConfig(CM_I2C_TypeDef* unit, uint32_t slot, uint32_t mode, uint32_t address);
void I2C_Cmd(CM_I2C_TypeDef* unit, unsigned state);
void I2C_IntCmd(CM_I2C_TypeDef* unit, uint32_t flags, unsigned state);
unsigned I2C_GetStatus(const CM_I2C_TypeDef* unit, uint32_t flags);
void I2C_ClearStatus(CM_I2C_TypeDef* unit, uint32_t flags);
void I2C_WriteData(CM_I2C_TypeDef* unit, uint8_t byte);
uint8_t I2C_ReadData(const CM_I2C_TypeDef* unit);

#define CM_RMU                    (&mock_rmu)
#define CM_PWC                    (&mock_pwc)
#define RMU_FLAG_ALL              0x7FFFU
#define RMU_RSTF0_SWRF            0x0100U
#define RMU_FLAG_SW               RMU_RSTF0_SWRF
#define READ_REG16_BIT(reg, bits) ((uint16_t)((reg) & (bits)))
#define __disable_irq()           ((void)0)
#define __DSB()                   ((void)0)
void NVIC_SystemReset(void);
void RMU_ClearStatus(void);
#endif
