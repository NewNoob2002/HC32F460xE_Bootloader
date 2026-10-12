#ifndef BQ40Z50_H
#define BQ40Z50_H
#include <stdint.h>

constexpr uint8_t kBq40z50DeviceAddress = 0x0BU;
constexpr uint8_t kBq40z50Temperature = 0x08U;
constexpr uint8_t kBq40z50Voltage = 0x09U;
constexpr uint8_t kBq40z50RelativeStateOfCharge = 0x0DU;
struct Bq40z50Sample {
    uint16_t temperature_dk; // SBS 0.1 K，保持面板线缆原始单位。
    uint16_t voltage_mv;
    uint16_t soc_percent;
};
/** @brief 读取并验证完整SBS样本；失败不修改sample，不通过默认0表示有效电量。仅主循环调用。 */
bool bq40z50_read_sample(Bq40z50Sample& sample);
#endif
