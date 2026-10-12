#ifndef APP_RECORDING_H
#define APP_RECORDING_H
#include <stdint.h>

struct AppRecordingSnapshot {
    bool active;
    bool operation_pending;
    uint32_t operation_sequence;
    bool allowed;
};
void app_recording_init();
/** @brief 主机同步不能覆盖尚未被完整响应读出确认的本地操作，禁止录制时不能同步为开始。 */
void app_recording_sync(bool active);
/** @brief 电源/低电量策略控制开始录制资格；禁止后仍允许停录制。 */
void app_recording_set_allowed(bool allowed);
/** @brief 仅在当前录制中产生一次停录制操作，重复调用不重复上报。 */
void app_recording_stop();
/** @brief 本地双击切换，生成一次待通知操作。 */
void app_recording_toggle();
AppRecordingSnapshot app_recording_snapshot();
/** @brief 只确认该响应所包含的操作版本，新发生操作仍保留。 */
void app_recording_confirm(uint32_t sequence);
#endif
