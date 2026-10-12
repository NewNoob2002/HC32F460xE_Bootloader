#ifndef PANEL_PROTOCOL_H
#define PANEL_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

constexpr size_t kPanelHeaderSize = 20U;
constexpr size_t kPanelCrcSize = 4U;
constexpr size_t kPanelMaxFrameSize = 256U;
constexpr uint32_t kPanelPartialTimeoutMs = 100U;

struct PanelFrame {
    uint16_t id;
    uint8_t type;
    const uint8_t* payload;
    uint16_t payload_size;
};
struct PanelParser {
    uint8_t bytes[kPanelMaxFrameSize];
    size_t size;
    size_t expected_size;
    uint32_t last_byte_ms;
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t timeouts;
};
using PanelFrameCallback = void (*)(const PanelFrame& frame, void* context);

/** @brief IEEE 反射 CRC32，初值/终异或 0xFFFFFFFF；data 非空或 size=0。 */
uint32_t panel_crc32(const uint8_t* data, size_t size);
/** @brief 有界字节解析；callback 的 frame/payload 仅在回调期间有效，允许连续帧和分段。 */
void panel_parser_push(PanelParser& parser, const uint8_t* data, size_t size, uint32_t now_ms,
                       PanelFrameCallback callback, void* context);
/** @brief 清除部分帧，不清除诊断计数或已发布响应；供超时/总线恢复调用。 */
void panel_parser_reset_partial(PanelParser& parser);
/** @brief 检查部分帧接收超时，时间比较允许 uint32_t 回绕。 */
void panel_parser_poll(PanelParser& parser, uint32_t now_ms);
/** @brief 编码固定20字节头及小端CRC；失败返回0，保留头字段显式为0。 */
size_t panel_encode_response(uint16_t id, uint8_t type, const uint8_t* payload, size_t payload_size, uint8_t* output,
                             size_t capacity);
#endif
