#include "panel_protocol.h"
#include <string.h>

namespace {
uint16_t read_le16(const uint8_t* bytes) {
    return static_cast<uint16_t>(static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U));
}
uint32_t read_le32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U)
           | (static_cast<uint32_t>(bytes[2]) << 16U) | (static_cast<uint32_t>(bytes[3]) << 24U);
}
} // namespace

uint32_t panel_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1U) ^ ((crc & 1U) ? UINT32_C(0xEDB88320) : 0U);
    }
    return crc ^ UINT32_MAX;
}

void panel_parser_reset_partial(PanelParser& parser) {
    parser.size = 0U;
    parser.expected_size = 0U;
}

void panel_parser_poll(PanelParser& parser, uint32_t now_ms) {
    if ((parser.size != 0U) && ((uint32_t)(now_ms - parser.last_byte_ms) >= kPanelPartialTimeoutMs)) {
        ++parser.timeouts;
        panel_parser_reset_partial(parser);
    }
}

void panel_parser_push(PanelParser& parser, const uint8_t* data, size_t size, uint32_t now_ms,
                       PanelFrameCallback callback, void* context) {
    if (data == nullptr)
        return;
    panel_parser_poll(parser, now_ms);
    constexpr uint8_t prefix[] = {0xAAU, 0x44U, 0x18U, 20U};
    for (size_t index = 0U; index < size; ++index) {
        const uint8_t byte = data[index];
        if ((parser.size < sizeof(prefix)) && (byte != prefix[parser.size])) {
            panel_parser_reset_partial(parser);
            if (byte != prefix[0])
                continue;
        }
        parser.bytes[parser.size++] = byte;
        parser.last_byte_ms = now_ms;
        if (parser.size == kPanelHeaderSize) {
            parser.expected_size = kPanelHeaderSize + kPanelCrcSize + read_le16(&parser.bytes[12]);
            if (parser.expected_size > sizeof(parser.bytes)) {
                ++parser.length_errors;
                panel_parser_reset_partial(parser);
                continue;
            }
        }
        if ((parser.expected_size == 0U) || (parser.size != parser.expected_size))
            continue;
        if (parser.size < kPanelHeaderSize + kPanelCrcSize) {
            panel_parser_reset_partial(parser);
            continue;
        }
        const size_t crc_offset = parser.size - kPanelCrcSize;
        if (panel_crc32(parser.bytes, crc_offset) == read_le32(&parser.bytes[crc_offset])) {
            ++parser.valid_frames;
            const PanelFrame frame{read_le16(&parser.bytes[4]), parser.bytes[17], &parser.bytes[kPanelHeaderSize],
                                   static_cast<uint16_t>(crc_offset - kPanelHeaderSize)};
            if (callback != nullptr)
                callback(frame, context);
        } else {
            ++parser.crc_errors;
        }
        panel_parser_reset_partial(parser);
    }
}

size_t panel_encode_response(uint16_t id, uint8_t type, const uint8_t* payload, size_t payload_size, uint8_t* output,
                             size_t capacity) {
    if ((output == nullptr) || ((payload == nullptr) && (payload_size != 0U))
        || (payload_size > kPanelMaxFrameSize - kPanelHeaderSize - kPanelCrcSize)
        || (capacity < kPanelHeaderSize + kPanelCrcSize + payload_size))
        return 0U;
    const size_t crc_offset = kPanelHeaderSize + payload_size;
    memset(output, 0, crc_offset + kPanelCrcSize);
    output[0] = 0xAAU;
    output[1] = 0x44U;
    output[2] = 0x18U;
    output[3] = 20U;
    output[4] = static_cast<uint8_t>(id);
    output[5] = static_cast<uint8_t>(id >> 8U);
    output[12] = static_cast<uint8_t>(payload_size);
    output[13] = static_cast<uint8_t>(payload_size >> 8U);
    output[16] = 3U;
    output[17] = type;
    if (payload_size != 0U)
        memcpy(&output[kPanelHeaderSize], payload, payload_size);
    const uint32_t crc = panel_crc32(output, crc_offset);
    for (size_t index = 0U; index < kPanelCrcSize; ++index)
        output[crc_offset + index] = static_cast<uint8_t>(crc >> (8U * index));
    return crc_offset + kPanelCrcSize;
}
