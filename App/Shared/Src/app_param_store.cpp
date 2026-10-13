#include "app_param_store.h"
#include <string.h>
#include "bsp_flash.h"
#include "firmware_layout.h"
#include "panel_protocol.h"

namespace {
constexpr uint32_t record_size = 16U;
constexpr uint32_t slot_count = APP_PARAM_SECTOR_SIZE / record_size;
constexpr uint32_t magic_version = 0x01503158U; // 小端 "X1P" + 版本 1。
static_assert(APP_PARAM_SECTOR_SIZE == BSP_FLASH_SECTOR_SIZE);
static_assert(APP_PARAM_SECTOR_BASE % BSP_FLASH_SECTOR_SIZE == 0U);
static_assert(APP_PARAM_SECTOR_SIZE % record_size == 0U);

struct Scan {
    uint32_t sequence;
    uint32_t next_slot;
    uint8_t offset;
    bool found;
};

uint32_t read_word(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U)
           | (static_cast<uint32_t>(bytes[2]) << 16U) | (static_cast<uint32_t>(bytes[3]) << 24U);
}
void put_word(uint8_t* bytes, uint32_t value) {
    for (uint32_t index = 0U; index < 4U; ++index)
        bytes[index] = static_cast<uint8_t>(value >> (8U * index));
}
bool read_record(uint32_t slot, uint8_t* record) {
    return slot < slot_count
           && bsp_flash_read(static_cast<uint32_t>(APP_PARAM_SECTOR_BASE) + slot * record_size, record, record_size)
                  == BSP_FLASH_OK;
}
bool scan_sector(Scan& scan) {
    scan = {};
    for (uint32_t slot = 0U; slot < slot_count; ++slot) {
        uint8_t record[record_size];
        if (!read_record(slot, record))
            return false;
        bool erased = true;
        for (uint8_t byte : record)
            erased = erased && (byte == 0xFFU);
        if (erased)
            continue;
        // 不复用空洞或半写槽位，避免对已编程的字再次执行 EFM_Program。
        scan.next_slot = slot + 1U;
        const uint32_t sequence = read_word(record + 4U);
        if ((read_word(record) != magic_version) || (sequence == 0U) || (record[8U] > 9U) || (record[9U] != 0U)
            || (record[10U] != 0U) || (record[11U] != 0U) || (read_word(record + 12U) != panel_crc32(record, 12U)))
            continue;
        if (!scan.found || (sequence >= scan.sequence)) {
            scan.found = true;
            scan.sequence = sequence;
            scan.offset = record[8U];
        }
    }
    return true;
}
} // namespace

bool app_param_store_load(uint8_t& offset) {
    offset = 0U;
    Scan scan{};
    if (!scan_sector(scan))
        return false;
    offset = scan.offset;
    return true;
}

bool app_param_store_save(uint8_t offset) {
    if (offset > 9U)
        return false;
    Scan scan{};
    if (!scan_sector(scan))
        return false;
    if (scan.found && (scan.offset == offset))
        return true;
    if (scan.next_slot == slot_count) {
        // ponytail: 单扇区擦除窗口掉电会丢偏移并恢复 0；需保护该窗口时改为双扇区。
        if (bsp_flash_erase_sector(APP_PARAM_SECTOR_BASE / BSP_FLASH_SECTOR_SIZE) != BSP_FLASH_OK || !scan_sector(scan)
            || (scan.next_slot != 0U))
            return false;
    }
    if ((scan.next_slot >= slot_count) || (scan.sequence == UINT32_MAX))
        return false;

    // EFM_Program 按 uint32_t 读取源；字数组保证对齐与类型，字节视图显式编码小端记录。
    uint32_t words[record_size / sizeof(uint32_t)]{};
    auto* record = reinterpret_cast<uint8_t*>(words);
    put_word(record, magic_version);
    put_word(record + 4U, scan.sequence + 1U);
    record[8U] = offset;
    put_word(record + 12U, panel_crc32(record, 12U));
    const uint32_t address = static_cast<uint32_t>(APP_PARAM_SECTOR_BASE) + scan.next_slot * record_size;
    if (bsp_flash_write(address, record, record_size) != BSP_FLASH_OK)
        return false;
    uint8_t readback[record_size];
    return read_record(scan.next_slot, readback) && (memcmp(record, readback, record_size) == 0);
}
