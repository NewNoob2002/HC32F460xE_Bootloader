#ifndef MP2762A_H
#define MP2762A_H
#include <stdint.h>
#include "bsp_i2c_master.h"

constexpr uint8_t kMp2762aDeviceAddress = 0x5CU;
struct Mp2762Status {
    bool input_present;
    uint8_t charge_state; // REG13[3:2]: 0停充、1预充、2快充、3终止。
};
/** @brief 探测地址并校验寄存器保留位；错误不等同器件不存在。 */
bsp_i2c_probe_result_t mp2762_probe();
/** @brief 禁止充电后配置旧产品参数：预充门槛6.6V、预充840mA、快充1600mA；每次写回读验证。 */
bool mp2762_configure();
/** @brief 仅修改CHG_EN，不改变供电BATTFET/OTG；失败返回false。 */
bool mp2762_set_charging(bool enabled);
/** @brief 读取REG13状态；失败不更新输出。 */
bool mp2762_read_status(Mp2762Status& status);
#endif
