#include "bsp_clock.h"
#include "hc32_ll.h"
void bsp_clock_init(void) {
    stc_clock_xtal_init_t xtal_init;
    stc_clock_pll_init_t mpll_init;

    GPIO_AnalogCmd(GPIO_PORT_H, (GPIO_PIN_00 | GPIO_PIN_01), ENABLE);
    (void)CLK_XtalStructInit(&xtal_init);
    (void)CLK_PLLStructInit(&mpll_init);

    /* Set bus clk div. */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4
                                      | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

    /* Config Xtal and enable Xtal */
    xtal_init.u8Mode = CLK_XTAL_MD_OSC;
    xtal_init.u8Drv = CLK_XTAL_DRV_ULOW;
    xtal_init.u8State = CLK_XTAL_ON;
    xtal_init.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&xtal_init);

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M). */
    mpll_init.PLLCFGR = 0UL;
    mpll_init.PLLCFGR_f.PLLM = 1UL - 1UL;
    mpll_init.PLLCFGR_f.PLLN = 50UL - 1UL;
    mpll_init.PLLCFGR_f.PLLP = 2UL - 1UL;
    mpll_init.PLLCFGR_f.PLLQ = 2UL - 1UL;
    mpll_init.PLLCFGR_f.PLLR = 2UL - 1UL;
    mpll_init.u8PLLState = CLK_PLL_ON;
    mpll_init.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&mpll_init);
    /* Wait MPLL ready. */
    uint32_t timeout = 0x100000;
    while ((CLK_GetStableStatus(CLK_STB_FLAG_PLL) != SET) && (timeout-- > 0UL)) {
        ;
    }

    /* sram init include read/write wait cycle setting */
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    /* flash read wait cycle setting */
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    /* 3 cycles for 126MHz ~ 200MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
    /* Switch driver ability */
    (void)PWC_HighSpeedToHighPerformance();
    /* Switch system clock source to MPLL. */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
    /* Reset cache ram */
    EFM_CacheRamReset(ENABLE);
    EFM_CacheRamReset(DISABLE);
    /* Enable cache */
    EFM_CacheCmd(ENABLE);

    SystemCoreClockUpdate();
}
