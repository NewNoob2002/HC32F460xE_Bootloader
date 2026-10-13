#ifndef APP_PARAM_STORE_H
#define APP_PARAM_STORE_H
#include <stdint.h>

/**
 * @brief 读取 APP_PARAM_SECTOR 中序号最大的有效 SOC 偏移（[0,9]）。
 * 擦除态/记录损坏按 0 返回；Flash 读取失败返回 false，offset 也为 0。
 * 16 字节小端记录：0..3="X1P\1"，4..7=序号，8=offset，9..11=0，12..15=前 12 字节的 CRC32。
 */
bool app_param_store_load(uint8_t& offset);
/**
 * @brief 追加偏移并回读验证；同值不写，越界/Flash 失败返回 false，调用方保留待保存标志。
 * 损坏/半写记录占用槽位，扇区满后擦除并从序号 1 重写；擦除窗口掉电后恢复默认 0。
 * 同一扇区序号不能回绕；UINT32_MAX 时拒绝追加，满后擦除才重置。
 * 仅主循环调用，不可重入；调用方按 D7 选择写入窗口，擦写会阻塞取指与中断。
 */
bool app_param_store_save(uint8_t offset);
#endif
