#include "app_runtime.h"
#include "bsp_timebase.h"

int main() {
    if (!app_runtime_init())
        app_runtime_fault();
    for (;;) {
        app_runtime_poll(bsp_time_ms());
    }
}
