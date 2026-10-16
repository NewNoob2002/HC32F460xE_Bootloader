#include "bq40z50.h"
#include "bsp_i2c_master.h"

bool bq40z50_read_sample(Bq40z50Sample& sample) {
    Bq40z50Sample next{};
    if (!bsp_i2c_master_read_word(kBq40z50DeviceAddress, kBq40z50Temperature, &next.temperature_dk)
        || !bsp_i2c_master_read_word(kBq40z50DeviceAddress, kBq40z50Voltage, &next.voltage_mv)
        || !bsp_i2c_master_read_word(kBq40z50DeviceAddress, kBq40z50RelativeStateOfCharge, &next.soc_percent)
        || (next.temperature_dk < 2332U) || (next.temperature_dk > 3982U) || (next.voltage_mv == 0U)
        || (next.voltage_mv == UINT16_MAX) || (next.soc_percent > 100U))
        return false;
    sample = next;
    return true;
}
