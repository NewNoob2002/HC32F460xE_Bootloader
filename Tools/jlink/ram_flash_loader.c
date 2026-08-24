#include <stdint.h>

#define EFM_FAPRT (*(volatile uint32_t *)0x40010400UL)
#define EFM_FRMC  (*(volatile uint32_t *)0x40010408UL)
#define EFM_FWMC  (*(volatile uint32_t *)0x4001040CUL)
#define EFM_FSR   (*(volatile uint32_t *)0x40010410UL)
#define EFM_FSCLR (*(volatile uint32_t *)0x40010414UL)
#define WDT_RR    (*(volatile uint32_t *)0x40049008UL)

#define EFM_CACHE_ALL       0x01010000UL
#define EFM_FWMC_PEMODE     0x00000001UL
#define EFM_FWMC_PEMOD      0x00000070UL
#define EFM_MODE_READ_ONLY  0x00000000UL
#define EFM_MODE_PROGRAM    0x00000010UL
#define EFM_MODE_ERASE      0x00000040UL
#define EFM_FLAG_ERRORS     0x0000002FUL
#define EFM_FLAG_OPTEND     0x00000010UL
#define EFM_FLAG_READY      0x00000100UL
#define EFM_CLEAR_FLAGS     0x0000003FUL

#define IMAGE_SOURCE        ((const volatile uint32_t *)0x20000000UL)
#ifndef IMAGE_LENGTH
#error "IMAGE_LENGTH must match the firmware binary"
#endif
#define IMAGE_ERASE_LENGTH  ((IMAGE_LENGTH + 0x1FFFUL) & ~0x1FFFUL)
#define STATUS_ADDRESS      0x20017000UL
#define LOADER_MAGIC        0x45464D32UL
#define LOADER_SUCCESS      0x600D600DUL
#define LOOP_TIMEOUT        10000000UL

_Static_assert((IMAGE_LENGTH > 0U) && (IMAGE_LENGTH <= 0x8000U) && ((IMAGE_LENGTH & 3U) == 0U),
               "firmware must be word-aligned and fit in Boot Flash");

typedef struct {
    uint32_t magic;
    uint32_t stage;
    uint32_t address;
    uint32_t fsr;
    uint32_t error;
    uint32_t expected;
    uint32_t actual;
} loader_status_t;

static volatile loader_status_t *const status =
    (volatile loader_status_t *)STATUS_ADDRESS;

static void watchdog_feed(void)
{
    WDT_RR = 0x0123UL;
    WDT_RR = 0x3210UL;
}

static int wait_complete(uint32_t address)
{
    uint32_t timeout = LOOP_TIMEOUT;

    while ((EFM_FSR & EFM_FLAG_READY) == 0U) {
        watchdog_feed();
        if (--timeout == 0U) {
            status->address = address;
            status->fsr = EFM_FSR;
            status->error = 1U;
            return 0;
        }
    }
    if ((EFM_FSR & EFM_FLAG_OPTEND) == 0U) {
        status->address = address;
        status->fsr = EFM_FSR;
        status->error = 2U;
        return 0;
    }
    if ((EFM_FSR & EFM_FLAG_ERRORS) != 0U) {
        status->address = address;
        status->fsr = EFM_FSR;
        status->error = 3U;
        return 0;
    }
    EFM_FSCLR = EFM_FLAG_OPTEND;
    return 1;
}

__attribute__((noreturn, used, section(".text.loader_main"))) void loader_main(void)
{
    uint32_t cache_state;
    uint32_t address;
    uint32_t index;

    __asm volatile("cpsid i" ::: "memory");
    status->magic = LOADER_MAGIC;
    status->stage = 1U;
    status->address = 0U;
    status->fsr = EFM_FSR;
    status->error = 0U;
    status->expected = 0U;
    status->actual = 0U;

    EFM_FAPRT = 0x0123U;
    EFM_FAPRT = 0x3210U;
    if (EFM_FAPRT != 1U) {
        status->error = 4U;
        goto done;
    }
    EFM_FWMC |= EFM_FWMC_PEMODE;
    if ((EFM_FWMC & EFM_FWMC_PEMODE) == 0U) {
        status->error = 5U;
        goto done;
    }

    cache_state = EFM_FRMC & EFM_CACHE_ALL;
    EFM_FRMC &= ~EFM_CACHE_ALL;

    status->stage = 2U;
    for (address = 0U; address < IMAGE_ERASE_LENGTH; address += 0x2000U) {
        EFM_FSCLR = EFM_CLEAR_FLAGS;
        EFM_FWMC = (EFM_FWMC & ~EFM_FWMC_PEMOD) | EFM_MODE_ERASE;
        *(volatile uint32_t *)address = 0U;
        if (!wait_complete(address)) {
            goto restore;
        }
    }

    status->stage = 3U;
    EFM_FWMC = (EFM_FWMC & ~EFM_FWMC_PEMOD) | EFM_MODE_PROGRAM;
    for (index = 0U; index < (IMAGE_LENGTH / 4U); ++index) {
        address = index * 4U;
        EFM_FSCLR = EFM_CLEAR_FLAGS;
        *(volatile uint32_t *)address = IMAGE_SOURCE[index];
        if (!wait_complete(address)) {
            goto restore;
        }
    }

    status->stage = 4U;
    EFM_FWMC = (EFM_FWMC & ~EFM_FWMC_PEMOD) | EFM_MODE_READ_ONLY;
    for (index = 0U; index < (IMAGE_LENGTH / 4U); ++index) {
        address = index * 4U;
        if (*(volatile uint32_t *)address != IMAGE_SOURCE[index]) {
            status->address = address;
            status->expected = IMAGE_SOURCE[index];
            status->actual = *(volatile uint32_t *)address;
            status->error = 6U;
            goto restore;
        }
    }
    status->stage = LOADER_SUCCESS;

restore:
    EFM_FWMC = (EFM_FWMC & ~(EFM_FWMC_PEMOD | EFM_FWMC_PEMODE)) |
               EFM_MODE_READ_ONLY;
    EFM_FRMC = (EFM_FRMC & ~EFM_CACHE_ALL) | cache_state;
    EFM_FRMC |= 0x01000000UL;
    EFM_FRMC &= ~0x01000000UL;
    EFM_FAPRT = 0U;

done:
    status->fsr = EFM_FSR;
    __asm volatile("dsb\n isb\n bkpt #0" ::: "memory");
    for (;;) {
        __asm volatile("nop");
    }
}
