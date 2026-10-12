#include "app_keys.h"
#include "app_power.h"
#include "app_recording.h"
#include "bsp_keys.h"
#include "bsp_timebase.h"
#include "multi_button.h"

namespace {
struct Key {
    Button button;
    uint32_t pressed_ms;
    bool held;
    bool long_fired;
};
Key keys[2]{};
uint32_t now_ms;
uint32_t last_tick_ms;

uint8_t read_key(uint8_t id) {
    return id == 1U ? static_cast<uint8_t>(bsp_keys_power_pressed())
                    : static_cast<uint8_t>(!bsp_keys_function_pressed());
}
void pressed(Button* button) {
    auto& key = keys[button->button_id - 1U];
    key.pressed_ms = now_ms;
    key.held = true;
    key.long_fired = false;
}
void released(Button* button) {
    keys[button->button_id - 1U].held = false;
}
void double_clicked(Button*) {
    if (app_power_snapshot().state == AppPowerState::Running)
        app_recording_toggle();
}
} // namespace

bool app_keys_init(uint32_t time_ms) {
    for (auto& key : keys)
        button_stop(&key.button);
    keys[0] = {};
    keys[1] = {};
    now_ms = last_tick_ms = time_ms;
    if (!bsp_keys_init())
        return false;
    for (uint8_t index = 0U; index < 2U; ++index) {
        auto* button = &keys[index].button;
        button_init(button, read_key, index == 0U ? 1U : 0U, static_cast<uint8_t>(index + 1U));
        button_attach(button, BTN_PRESS_DOWN, pressed);
        button_attach(button, BTN_PRESS_UP, released);
        if (index == 1U)
            button_attach(button, BTN_DOUBLE_CLICK, double_clicked);
        if (button_start(button) != 0)
            return false;
    }
    return true;
}
void app_keys_poll(uint32_t time_ms) {
    now_ms = time_ms;
    if (bsp_time_elapsed(now_ms, last_tick_ms, TICKS_INTERVAL)) {
        last_tick_ms = now_ms;
        button_ticks();
    }
    for (uint8_t index = 0U; index < 2U; ++index) {
        auto& key = keys[index];
        // 先确认原始电平仍按下，释放消抖窗口不能越过长按门槛触发产品操作。
        const bool raw_pressed = index == 0U ? bsp_keys_power_pressed() : bsp_keys_function_pressed();
        if (key.held && raw_pressed && !key.long_fired
            && bsp_time_elapsed(now_ms, key.pressed_ms, index == 0U ? 2000U : 8000U)) {
            key.long_fired = true;
            if (index == 0U)
                app_power_key_long_press(now_ms);
            else
                app_power_force_shutdown();
        }
    }
}
