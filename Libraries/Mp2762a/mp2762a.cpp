#include "mp2762a.h"

namespace {
bool read_byte(uint8_t reg, uint8_t& value) {
    return bsp_i2c_master_read(kMp2762aDeviceAddress, reg, &value, 1U);
}
bool update_bits(uint8_t reg, uint8_t mask, uint8_t bits) {
    uint8_t value;
    if (!read_byte(reg, value))
        return false;
    // MP2762A Rev1.1 REG08：读改写不得再次触发 REG_RST/WTD_RST。
    if (reg == 0x08U)
        value &= 0x3EU;
    const uint32_t inv_mask = static_cast<uint32_t>(mask) ^ 0xFFU;
    value = static_cast<uint8_t>((static_cast<uint32_t>(value) & inv_mask) | static_cast<uint32_t>(bits));
    uint8_t readback;
    return bsp_i2c_master_write_byte(kMp2762aDeviceAddress, reg, value) && read_byte(reg, readback)
           && ((readback & mask) == (value & mask));
}
} // namespace

bsp_i2c_probe_result_t mp2762_probe() {
    const auto result = bsp_i2c_master_probe(kMp2762aDeviceAddress);
    if (result != BSP_I2C_PROBE_PRESENT)
        return result;
    uint8_t input_limit, config, status;
    // 数据手册Rev1.1 pp32/36/42的保留位；无独立器件ID，拓扑假设需产品确认。
    if (!read_byte(0x00U, input_limit) || !read_byte(0x08U, config) || !read_byte(0x13U, status)
        || ((input_limit & 0x80U) != 0U) || ((config & 1U) != 0U) || ((status & 0x20U) != 0U))
        return BSP_I2C_PROBE_IO_ERROR;
    return BSP_I2C_PROBE_PRESENT;
}

bool mp2762_set_charging(bool enabled) {
    return update_bits(0x08U, 0x10U, enabled ? 0x10U : 0U);
}

bool mp2762_configure() {
    // REG07/30选择6.6V；REG03最大840mA，旧880mA请求实际也截断编码为840mA。
    // 不执行全寄存器复位，不改变OTP电池满充电压和系统供电相关配置。
    return mp2762_set_charging(false) && update_bits(0x07U, 0x30U, 0U) && update_bits(0x30U, 0x08U, 0x08U)
           && update_bits(0x03U, 0xF0U, 0xF0U) && update_bits(0x02U, 0x7FU, 32U);
}

bool mp2762_read_status(Mp2762Status& status) {
    uint8_t value;
    if (!read_byte(0x13U, value) || ((value & 0x20U) != 0U))
        return false;
    status.input_present = (value & 0x02U) != 0U;
    status.charge_state = static_cast<uint8_t>((static_cast<uint32_t>(value) >> 2U) & 3U);
    return true;
}
