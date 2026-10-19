#include "app_recording.h"

namespace {
bool active;
bool allowed;
uint32_t generated;
uint32_t confirmed;
} // namespace
void app_recording_init() {
    active = false;
    allowed = true;
    generated = 0U;
    confirmed = 0U;
}
void app_recording_sync(bool state) {
    if ((generated == confirmed) && (!state || allowed))
        active = state;
}
void app_recording_set_allowed(bool state) {
    allowed = state;
}
void app_recording_stop() {
    if (active) {
        active = false;
        ++generated;
    }
}
void app_recording_toggle() {
    if (!active && !allowed)
        return;
    active = !active;
    ++generated;
}
AppRecordingSnapshot app_recording_snapshot() {
    return {active, generated != confirmed, generated, allowed};
}
void app_recording_confirm(uint32_t sequence) {
    confirmed = sequence;
}
