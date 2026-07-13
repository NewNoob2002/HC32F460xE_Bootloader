#include "boot_timebase.h"
#include <assert.h>
void test_boot_time(void)
{
    assert(!boot_time_elapsed(99U, 0U, 100U));
    assert(boot_time_elapsed(100U, 0U, 100U));
    assert(!boot_time_elapsed(3U, UINT32_MAX - 5U, 10U));
    assert(boot_time_elapsed(4U, UINT32_MAX - 5U, 10U));
}
