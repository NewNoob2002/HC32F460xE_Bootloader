#include <stdio.h>
#include "SEGGER_RTT.h"
#include "boot_timebase.h"
#include "elog.h"
ElogErrCode elog_port_init(void) {
    SEGGER_RTT_Init();
    (void)SEGGER_RTT_SetFlagsUpBuffer(0U, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    return ELOG_NO_ERR;
}
void elog_port_deinit(void) {}
void elog_port_output(const char* log, size_t size) {
    (void)SEGGER_RTT_Write(0U, log, (unsigned)size);
}
void elog_port_output_lock(void) {}
void elog_port_output_unlock(void) {}
const char* elog_port_get_time(void) {
    static char buffer[11];
    (void)snprintf(buffer, sizeof(buffer), "%010lu", (unsigned long)boot_time_ms());
    return buffer;
}
const char* elog_port_get_p_info(void) {
    return "";
}
const char* elog_port_get_t_info(void) {
    return "";
}
