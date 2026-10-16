#include "hc32f460_it.h"
#include <stdbool.h>
#include "bsp_status_led.h"
#include "hc32f460.h"

volatile boot_fault_snapshot_t g_boot_fault_snapshot __attribute__((section(".noinit.boot_fault"), used));

static inline void cpu_nop(void) {
    __asm volatile("nop");
}

static inline void cpu_disable_irq(void) {
    __asm volatile("cpsid i" : : : "memory");
}

static inline uint32_t cpu_get_msp(void) {
    uint32_t result;
    __asm volatile("mrs %0, msp" : "=r"(result));
    return result;
}

static inline uint32_t cpu_get_psp(void) {
    uint32_t result;
    __asm volatile("mrs %0, psp" : "=r"(result));
    return result;
}

static inline uint32_t cpu_get_control(void) {
    uint32_t result;
    __asm volatile("mrs %0, control" : "=r"(result));
    return result;
}

static inline uint32_t cpu_get_primask(void) {
    uint32_t result;
    __asm volatile("mrs %0, primask" : "=r"(result));
    return result;
}

static inline uint32_t cpu_get_basepri(void) {
    uint32_t result;
    __asm volatile("mrs %0, basepri" : "=r"(result));
    return result;
}

static inline uint32_t cpu_get_faultmask(void) {
    uint32_t result;
    __asm volatile("mrs %0, faultmask" : "=r"(result));
    return result;
}

static inline void cpu_dsb(void) {
    __asm volatile("dsb 0xf" : : : "memory");
}

static inline void cpu_isb(void) {
    __asm volatile("isb 0xf" : : : "memory");
}

static inline void cpu_bkpt(void) {
    __asm volatile("bkpt 0");
}

void NMI_Handler(void) {
    bsp_status_led_fault_on();
    while (1) {
        cpu_nop();
    }
}

__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile("tst lr, #4\n"
                   "ite eq\n"
                   "mrseq r0, msp\n"
                   "mrsne r0, psp\n"
                   "mov r1, lr\n"
                   "b boot_hardfault_capture\n");
}

__attribute__((noreturn)) void boot_hardfault_capture(uint32_t* frame, uint32_t exc_return) {
    cpu_disable_irq();
    if ((exc_return & (1UL << 4U)) == 0U)
        frame += 18U; /* Skip S0-S15, FPSCR, and the reserved word. */
    g_boot_fault_snapshot.magic = BOOT_FAULT_SNAPSHOT_MAGIC;
    g_boot_fault_snapshot.exc_return = exc_return;
    g_boot_fault_snapshot.msp = cpu_get_msp();
    g_boot_fault_snapshot.psp = cpu_get_psp();
    g_boot_fault_snapshot.frame_address = (uint32_t)frame;
    uint32_t frame_address = (uint32_t)frame;
    bool frame_valid = ((frame_address >= 0x1FFF8000UL) && (frame_address <= (0x20027000UL - 32UL)))
                       || ((frame_address >= 0x200F0000UL) && (frame_address <= (0x200F1000UL - 32UL)));
    g_boot_fault_snapshot.frame_valid = frame_valid ? 1UL : 0UL;
    if (frame_valid) {
        g_boot_fault_snapshot.stacked_r0 = frame[0];
        g_boot_fault_snapshot.stacked_r1 = frame[1];
        g_boot_fault_snapshot.stacked_r2 = frame[2];
        g_boot_fault_snapshot.stacked_r3 = frame[3];
        g_boot_fault_snapshot.stacked_r12 = frame[4];
        g_boot_fault_snapshot.stacked_lr = frame[5];
        g_boot_fault_snapshot.stacked_pc = frame[6];
        g_boot_fault_snapshot.stacked_xpsr = frame[7];
    } else {
        g_boot_fault_snapshot.stacked_r0 = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_r1 = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_r2 = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_r3 = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_r12 = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_lr = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_pc = 0xFFFFFFFFUL;
        g_boot_fault_snapshot.stacked_xpsr = 0xFFFFFFFFUL;
    }
    g_boot_fault_snapshot.cfsr = SCB->CFSR;
    g_boot_fault_snapshot.hfsr = SCB->HFSR;
    g_boot_fault_snapshot.dfsr = SCB->DFSR;
    g_boot_fault_snapshot.afsr = SCB->AFSR;
    g_boot_fault_snapshot.mmfar = SCB->MMFAR;
    g_boot_fault_snapshot.bfar = SCB->BFAR;
    g_boot_fault_snapshot.shcsr = SCB->SHCSR;
    g_boot_fault_snapshot.cpacr = SCB->CPACR;
    g_boot_fault_snapshot.control = cpu_get_control();
    g_boot_fault_snapshot.primask = cpu_get_primask();
    g_boot_fault_snapshot.basepri = cpu_get_basepri();
    g_boot_fault_snapshot.faultmask = cpu_get_faultmask();
    g_boot_fault_snapshot.icsr = SCB->ICSR;
    g_boot_fault_snapshot.vtor = SCB->VTOR;
    bsp_status_led_fault_on();
    cpu_dsb();
    cpu_isb();
    cpu_bkpt();
    while (1) {
        cpu_nop();
    }
}
