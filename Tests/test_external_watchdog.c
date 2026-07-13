#include "bsp_external_watchdog.h"
#include <assert.h>
typedef struct { bool level; unsigned writes; } watchdog_mock_t;
uint32_t boot_time_ms(void) { return 0U; }
static void write_watchdog(bool level, void *context) { watchdog_mock_t *mock = context; mock->level = level; ++mock->writes; }
void test_external_watchdog(void)
{
    watchdog_scheduler_t scheduler;
    watchdog_mock_t mock = {false, 0U};
    watchdog_scheduler_init(&scheduler, 0U, 3000U, 2U, true, write_watchdog, &mock);
    assert(!mock.level && mock.writes == 1U);
    watchdog_scheduler_poll(&scheduler, 2999U); assert(mock.writes == 1U);
    watchdog_scheduler_poll(&scheduler, 3000U); assert(mock.level && mock.writes == 2U);
    watchdog_scheduler_poll(&scheduler, 3002U); assert(!mock.level && mock.writes == 3U);
    watchdog_scheduler_poll(&scheduler, 6000U); assert(mock.level && mock.writes == 4U);
    watchdog_scheduler_poll(&scheduler, 9000U); assert(!mock.level && mock.writes == 5U);
    watchdog_scheduler_poll(&scheduler, 9000U); assert(mock.level && mock.writes == 6U);
    watchdog_scheduler_force(&scheduler, 9000U); assert(mock.writes == 6U);
    watchdog_scheduler_init(&scheduler, UINT32_MAX - 1000U, 3000U, 1U, false, write_watchdog, &mock);
    watchdog_scheduler_poll(&scheduler, 1998U); assert(mock.level);
    watchdog_scheduler_poll(&scheduler, 1999U); assert(!mock.level);
    watchdog_scheduler_init(&scheduler, 0U, 0U, 0U, true, write_watchdog, &mock); assert(!scheduler.enabled);
}
