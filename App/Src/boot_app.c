#include "boot_app.h"
void boot_app_select_mode(boot_context_t* context, bool app_valid, bool software_reset) {
    if (!app_valid)
        context->mode = BOOT_MODE_RECOVERY;
    else if (software_reset)
        context->mode = BOOT_MODE_UPDATE_WINDOW;
    else
        context->mode = BOOT_MODE_START_APPLICATION;
}
