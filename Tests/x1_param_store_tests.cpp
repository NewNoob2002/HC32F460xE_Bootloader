#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "app_param_store.h"
#include "bsp_flash.h"
#include "firmware_layout.h"
#include "panel_protocol.h"

namespace {
constexpr uint32_t record_size = 16U;
constexpr uint32_t slot_count = (APP_PARAM_DATA_END - APP_PARAM_SECTOR_BASE) / record_size;
static_assert(slot_count == 509U);
// 用 Python struct/zlib 独立生成：版本 1、序号 1、offset 3、IEEE CRC32。
constexpr uint8_t golden[] = {0x58U, 0x31U, 0x50U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U,
                              0x03U, 0x00U, 0x00U, 0x00U, 0x3AU, 0x43U, 0x36U, 0xD8U};
uint8_t flash[APP_PARAM_SECTOR_SIZE];
uint32_t reads, writes, erases, last_write, fail_read_call;
uint32_t write_bytes;
bool write_fails, corrupt_write, erase_fails, incomplete_erase;
bool fail_verify, corrupt_verify, next_read_fails, next_read_corrupt;

void check(bool ok) {
    if (!ok)
        std::abort();
}
void reset() {
    memset(flash, 0xFF, sizeof(flash));
    reads = writes = erases = last_write = fail_read_call = 0U;
    write_bytes = record_size;
    write_fails = corrupt_write = erase_fails = incomplete_erase = false;
    fail_verify = corrupt_verify = next_read_fails = next_read_corrupt = false;
}
void check_range(uint32_t address, uint32_t length) {
    check(address >= APP_PARAM_SECTOR_BASE && address < 0x0007FFDCUL);
    check(length == record_size && length <= 0x0007FFDCUL - address);
    check((address - APP_PARAM_SECTOR_BASE) % record_size == 0U);
}
void put_word(uint8_t* bytes, uint32_t value) {
    for (uint32_t index = 0U; index < 4U; ++index)
        bytes[index] = static_cast<uint8_t>(value >> (8U * index));
}
void crc(uint8_t* record) {
    put_word(record + 12U, panel_crc32(record, 12U));
}
void seed(uint32_t slot, uint32_t sequence, uint8_t offset) {
    check(slot < slot_count);
    auto* record = flash + static_cast<size_t>(slot) * record_size;
    memcpy(record, golden, sizeof(golden));
    put_word(record + 4U, sequence);
    record[8U] = offset;
    crc(record);
}
void expect_offset(uint8_t expected) {
    uint8_t offset = 0xFFU;
    check(app_param_store_load(offset));
    check(offset == expected);
}
} // namespace

extern "C" {
int32_t bsp_flash_read(uint32_t address, uint8_t* data, uint32_t length) {
    check_range(address, length);
    check(data != nullptr);
    ++reads;
    if (next_read_fails || (reads == fail_read_call)) {
        next_read_fails = false;
        return -1;
    }
    memcpy(data, flash + (address - APP_PARAM_SECTOR_BASE), length);
    if (next_read_corrupt) {
        next_read_corrupt = false;
        data[0U] ^= 1U;
    }
    return BSP_FLASH_OK;
}
int32_t bsp_flash_write(uint32_t address, const uint8_t* data, uint32_t length) {
    check_range(address, length);
    check(data != nullptr && reinterpret_cast<uintptr_t>(data) % alignof(uint32_t) == 0U);
    auto* target = flash + (address - APP_PARAM_SECTOR_BASE);
    for (uint32_t index = 0U; index < length; ++index)
        check(target[index] == 0xFFU); // 不重用半写记录，保守地禁止任何重编程。
    ++writes;
    last_write = address;
    check(write_bytes <= length);
    for (uint32_t index = 0U; index < write_bytes; ++index)
        target[index] &= data[index];
    if (corrupt_write)
        target[8U] = 0U;
    next_read_fails = fail_verify;
    next_read_corrupt = corrupt_verify;
    return write_fails || (write_bytes != length) ? -1 : BSP_FLASH_OK;
}
int32_t bsp_flash_erase_sector(uint32_t sector) {
    check(sector == APP_PARAM_SECTOR_BASE / BSP_FLASH_SECTOR_SIZE);
    ++erases;
    if (erase_fails)
        return -1;
    memset(flash, 0xFF, incomplete_erase ? record_size : sizeof(flash));
    return BSP_FLASH_OK;
}
} // extern "C"

int main() {
    reset();
    expect_offset(0U);
    check(reads == slot_count && writes == 0U && erases == 0U);
    check(app_param_store_save(3U));
    check(writes == 1U && erases == 0U && last_write == APP_PARAM_SECTOR_BASE);
    check(memcmp(flash, golden, sizeof(golden)) == 0);
    expect_offset(3U);
    check(app_param_store_save(3U));
    check(writes == 1U && erases == 0U); // 同值经重新扫描验证后不写。
    check(!app_param_store_save(10U) && !app_param_store_save(0xFFU));
    check(writes == 1U);
    check(app_param_store_save(9U));
    check(last_write == APP_PARAM_SECTOR_BASE + record_size);
    expect_offset(9U);
    check(app_param_store_save(0U));
    expect_offset(0U); // 显式的零偏移也能覆盖旧值。

    // 扫描整个扇区：空洞/乱序不提前终止，最大有效序号优先。
    reset();
    seed(0U, 5U, 2U);
    seed(3U, 3U, 4U);
    seed(7U, 6U, 9U);
    seed(11U, 7U, 5U);
    flash[11U * record_size + 12U] ^= 1U;
    expect_offset(9U);
    check(app_param_store_save(6U));
    check(last_write == APP_PARAM_SECTOR_BASE + 12UL * record_size);
    expect_offset(6U);
    seed(15U, 7U, 7U); // 相同序号时取后面的有效记录。
    expect_offset(7U);

    // 版本/偏移/保留字段/零序号即使 CRC 正确也不能生效。
    for (unsigned invalid = 0U; invalid < 4U; ++invalid) {
        reset();
        seed(0U, 1U, 3U);
        seed(1U, 2U, 5U);
        auto* record = flash + record_size;
        if (invalid == 0U)
            record[3U] = 2U;
        if (invalid == 1U)
            record[8U] = 10U;
        if (invalid == 2U)
            record[9U] = 1U;
        if (invalid == 3U)
            put_word(record + 4U, 0U);
        crc(record);
        expect_offset(3U);
    }
    reset();
    seed(0U, 1U, 3U);
    flash[12U] ^= 1U;
    expect_offset(0U);
    check(app_param_store_save(2U));
    check(last_write == APP_PARAM_SECTOR_BASE + record_size);
    expect_offset(2U);

    // 每个 32-bit 编程边界掉电：旧记录保留，重试跳过半写槽。
    for (uint32_t prefix = 4U; prefix < record_size; prefix += 4U) {
        reset();
        check(app_param_store_save(3U));
        write_bytes = prefix;
        check(!app_param_store_save(5U));
        expect_offset(3U);
        write_bytes = record_size;
        check(app_param_store_save(5U));
        check(last_write == APP_PARAM_SECTOR_BASE + 2UL * record_size);
        expect_offset(5U);
    }

    // 编程前失败不占槽，完整编程后报错则在重试扫描时确认，均不伪报成功。
    reset();
    check(app_param_store_save(3U));
    write_bytes = 0U;
    check(!app_param_store_save(5U));
    expect_offset(3U);
    write_bytes = record_size;
    write_fails = true;
    check(!app_param_store_save(5U));
    write_fails = false;
    check(app_param_store_save(5U));
    check(writes == 3U && last_write == APP_PARAM_SECTOR_BASE + record_size);
    expect_offset(5U);

    // 静默坏写、回读失败、回读字节不一致均返回失败。
    reset();
    check(app_param_store_save(3U));
    corrupt_write = true;
    check(!app_param_store_save(5U));
    expect_offset(3U);
    corrupt_write = false;
    check(app_param_store_save(5U));
    check(last_write == APP_PARAM_SECTOR_BASE + 2UL * record_size);
    for (unsigned failure = 0U; failure < 2U; ++failure) {
        reset();
        check(app_param_store_save(3U));
        fail_verify = failure == 0U;
        corrupt_verify = failure == 1U;
        check(!app_param_store_save(5U));
        fail_verify = corrupt_verify = false;
        expect_offset(5U); // Flash 记录实际完整，上电恢复无需依赖失败调用的 RAM 状态。
        check(app_param_store_save(5U));
        check(writes == 2U && erases == 0U);
    }

    // 任一槽读取失败都按失败返回，不使用不完整扫描结果进行擦写。
    const uint32_t failed_slots[] = {0U, slot_count - 1U};
    for (uint32_t failed_slot : failed_slots) {
        reset();
        seed(0U, 1U, 3U);
        fail_read_call = failed_slot + 1U;
        uint8_t offset = 9U;
        check(!app_param_store_load(offset) && offset == 0U);
        fail_read_call = reads + failed_slot + 1U;
        check(!app_param_store_save(5U));
        check(writes == 0U && erases == 0U);
        expect_offset(3U);
    }

    // 控制尾部即使看起来是有效新记录，也不能作为业务数据扫描。
    reset();
    seed(0U, 1U, 3U);
    auto* control_tail = flash + 0x1FE0U;
    memcpy(control_tail, golden, sizeof(golden));
    put_word(control_tail + 4U, UINT32_MAX);
    control_tail[8U] = 9U;
    crc(control_tail);
    expect_offset(3U);

    // 写满 509 条才擦除；同值、擦除失败及未完整擦除均不能误写。
    reset();
    for (uint32_t slot = 0U; slot < slot_count; ++slot)
        check(app_param_store_save(static_cast<uint8_t>((slot + 1U) % 2U)));
    check(writes == slot_count && erases == 0U);
    check(last_write == 0x0007FFC0UL);
    check(app_param_store_save(1U));
    check(erases == 0U && writes == slot_count);
    erase_fails = true;
    check(!app_param_store_save(9U));
    check(writes == slot_count);
    expect_offset(1U);
    erase_fails = false;
    incomplete_erase = true;
    check(!app_param_store_save(9U));
    check(writes == slot_count);
    expect_offset(1U);
    incomplete_erase = false;
    check(app_param_store_save(9U));
    check(erases == 3U && writes == slot_count + 1U && last_write == APP_PARAM_SECTOR_BASE);
    expect_offset(9U);
    for (uint32_t index = record_size; index < sizeof(flash); ++index)
        check(flash[index] == 0xFFU);

    // 擦除后掉电/首条写失败安全回退 0；重试仍从擦除后的首槽开始。
    reset();
    memset(flash, 0, sizeof(flash));
    write_bytes = 0U;
    check(!app_param_store_save(7U));
    check(erases == 1U);
    expect_offset(0U);
    write_bytes = record_size;
    check(app_param_store_save(7U));
    check(erases == 1U && last_write == APP_PARAM_SECTOR_BASE);
    expect_offset(7U);

    // 不允许序号溢出使新记录永久被旧大序号覆盖。
    reset();
    seed(0U, UINT32_MAX, 3U);
    expect_offset(3U);
    check(!app_param_store_save(4U));
    check(writes == 0U && erases == 0U);

    std::puts("x1_param_store_tests: golden / recovery / bounds / torn write / failures / full sector PASS");
    return 0;
}
