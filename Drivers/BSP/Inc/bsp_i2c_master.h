#ifndef BSP_I2C_MASTER_H
#define BSP_I2C_MASTER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 配置 X1 I2C2 PA9/SCL、PA8/SDA、100 kHz，仅主循环调用；初始化时须解锁 GPIO/FCG。 */
bool bsp_i2c_master_init(void);
typedef enum { BSP_I2C_PROBE_PRESENT, BSP_I2C_PROBE_ADDRESS_NACK, BSP_I2C_PROBE_IO_ERROR } bsp_i2c_probe_result_t;
/** @brief 单个7-bit地址探测，明确区分地址NACK与超时/总线错误；不扫描总线。 */
bsp_i2c_probe_result_t bsp_i2c_master_probe(uint8_t address);
/** @brief 有界寄存器读，size仅1或2；失败不更新data。 */
bool bsp_i2c_master_read(uint8_t address, uint8_t command, uint8_t* data, uint8_t size);
/** @brief 有界8-bit寄存器写；调用方须负责器件电气范围和写后验证。 */
bool bsp_i2c_master_write_byte(uint8_t address, uint8_t command, uint8_t value);
/**
 * @brief SMBus 只读 word：7-bit address、command，返回小端16位数据。
 * @param value 非空；失败不更新输出。不支持 PEC；仅用于当前 BQ40Z50 无 PEC 读命令。
 * @note 同步但有界，保持中断开放；失败尝试 STOP 并重置控制器，不产生恢复时钟或扫描。
 */
bool bsp_i2c_master_read_word(uint8_t address, uint8_t command, uint16_t* value);
#ifdef __cplusplus
}
#endif
#endif
