# 工程结构与 App 接入

当前 Boot 和升级流程由用户完成过测试验证。本次调整集中在目录、构建依赖和共用驱动边界，升级协议、Flash 操作和启动决策保持原逻辑。X1 已接入 C++17 可烧录测试镜像及基础HIL，支持面板轮询、BQ采样和运行时识别的双版本充电；X1Pro/XBase仍只有配置入口。完整产品功能与电气验收未完成，见 [App构建说明](../App/README.md) 和 [HIL记录](x1_hil_test_version.md)。

```text
Boot/                  Boot 入口、启动/升级/交接策略、异常处理、LED 模式
Common/Inc/            共用 Flash/SRAM 地址契约、日志宏
Common/Protocol/       独立帧解析与 CRC；不含升级执行或 BSP 依赖
Drivers/BSP/           共用硬件驱动、板级参数、时基及 EasyLogger 端口
Drivers/CMSIS/         原有 HC32 startup/system 与 CMSIS
Drivers/hc32f4xx_ll_drivers/  原有厂商 SDK
Boot/Linker/           Boot 专用链接脚本（32 KiB，含 ICG）
App/Linker/            三产品共用的 App 链接脚本（456 KiB，不含 ICG）
Platform/              HC32 构建组合与 syscalls；保留 SVD；公共链接段位于 HC32F460/Linker/
Libraries/             第三方库；当前按需组合 RTT/EasyLogger
App/Products/X1/       无屏幕产品配置及 main.cpp 编排入口
App/Shared/            共用运行时、面板/录制/电池/按键/电源业务、公开接口与异常处理
App/Shared/Protocol/   独立CRC32面板帧解析与响应编码
App/Products/X1Pro/    OLED/LVGL 产品配置及后续专属入口
App/Products/XBase/    继承 X1Pro 基线，独立维护增减项
Tests/                 Boot 回归及三个产品的共用驱动检查
Tools/                 现有合并镜像与 J-Link 工具
```

## 依赖与所有权

- `firmware::config` 只公开 `Common/Inc`，Boot/App 共用同一地址契约。
- `firmware::protocol` 只公开协议头文件；保留 `BootProtocol*` API 名称以保持现有调用和线缆协议兼容。收到的有效帧由镜像自行处理，App 不链接 Boot 升级服务。
- `hc32::headers` 提供 HC32 SDK/BSP 头文件与原有 CPU/编译选项；`hc32::platform` 将 startup/system、BSP 和所需 LL 源码编译进消费它的镜像。每个镜像各自拥有驱动静态状态。
- `firmware::logging` 按需提供 RTT/EasyLogger 及 `bsp_log_port.c`。日志端口使用共用 `bsp_time_ms()`；`FIRMWARE_CFG_LOGGING` 决定共用代码日志开关。
- Boot 的 `BOOT_CFG_LOGGING` 与共用日志开关由 Boot target 同步设置；`BOOT_ENABLE_LOGGING` 和原有 preset 保持可用。
- `bsp_status_led_*` 只负责 GPIO、有效电平与故障常亮；`boot_status_led_*` 和原调度器拥有 Boot 的更新/恢复/错误指示模式。App 通过 GPIO 接口实现自己的提示策略。
- `bsp_reset_*` 读取复位原因并提供普通软件复位入口；一次性升级标记和升级后复位由 `boot_reset_*` 拥有。App调用`bsp_reset_request()`，不写Boot升级完成标记。
- ICG 只加入 Boot target，固定链接在 `0x400`；App 复用平台时不会重复携带 ICG。

共用代码不包含 `Boot/Inc`，不引用 Boot 配置、LED 模式或升级状态。暂不增加升级管理器抽象、协议分发注册表或新 MCU 适配层；后续跨 MCU 工作仍按 [portability_plan.md](portability_plan.md) 推进。

## 三个产品

| 产品目标 | 配置文件 | OLED | LVGL | 业务基线 |
|---|---|---|---|---|
| `app::X1` | `App/Products/X1/product.cmake` | 关闭 | 关闭 | 开关机、LED 提示、充电 |
| `app::X1Pro` | `App/Products/X1Pro/product.cmake` | 开启 | 开启 | 共用逻辑与显示业务 |
| `app::XBase` | `App/Products/XBase/product.cmake` | 继承 X1Pro | 继承 X1Pro | 复用 X1Pro，集中维护差异 |

这些是产品配置目标；X1 另有 `hc32f460_app_x1` 可执行镜像。目标导出 `APP_PRODUCT_NAME`、`APP_HAS_OLED`、`APP_USE_LVGL`；一个实际 App 只能选择一个产品目标。当前所有 Boot 构建都不包含 OLED/LVGL。App 接入时必须按配置选择显示源码和库，不能仅用 C 宏关闭调用而仍编译整个显示库。

### 后续源码放置

- 开关机、LED 提示和充电等三产品共用业务放在 `App/Shared/`。
- X1Pro/XBase 共用的显示业务放在 `App/Shared/Display/`，产品差异留在各自 `App/Products/<产品>/`。
- 不复制三套 BSP、协议或共用业务；每个产品保留自己的入口、源码清单与配置。
- `Libraries/Bq40z50`、`Libraries/Mp2762a` 已移除Arduino依赖，使用App I2C2 BSP，由 `firmware::battery_devices` 按需链接；Boot不加入充电芯片或屏幕依赖。
- X1按键复用 `Libraries/MultiButton` 的C库，事件适配位于`App/Shared/Src/app_keys.cpp`，GPIO输入位于`bsp_keys.c`，电源时长与状态位于`app_power.cpp`；Boot不编译按键库。
- 当前 `Libraries` 中没有 LVGL 或 OLED 实现；接入 X1Pro/XBase 时需提供匹配的库和显示驱动，不能把已有历史 ST7789 构建记录当作 OLED 硬件契约。

### App 链接与启动要求

Boot 使用 `Boot/Linker/HC32F460_boot.ld`，固定链接 `0x00000000` 起的 **32 KiB**，保留 `0x400` 的 ICG。三个 App 共用 `App/Linker/HC32F460_app.ld`，从 `0x00008000` 起，到 `0x0007A000` 前结束（456 KiB），保留末尾三块 8 KiB 区域，并在误加入 ICG 时拒绝链接。

两者通过 `INCLUDE` 复用 `Platform/HC32F460/Linker/HC32F460_sections.ld` 中的代码、初始化数据、BSS、noinit、栈/堆和保留 SRAM 段。公共段同时检查 RAM 容量和 Flash 装载内容的末地址，初始化数据也不能越过镜像边界。三个脚本的改动会触发相应镜像重新链接。

实际 App target 应链接对应的 `app::<产品>` 和 `app::image`；后者自动选择 App 链接脚本、公共段搜索路径、`hc32::platform` 和 `VECT_TAB_OFFSET=0x00008000U`。主机测试只链接产品配置，不继承这些镜像选项。按实际需要加入 `firmware::protocol`、`firmware::logging`，自行设置日志开关和独立镜像输出名。先完成硬件与运行时初始化，再按交接契约恢复中断；当前共用 `bsp_timebase` 独占 SysTick，若接入 RTOS 则改由 RTOS 统一提供时基，避免重复定义 SysTick Handler。

当前 BSP 参数沿用已验证的 Boot 板卡：PB3 电源保持、PA6 看门狗、PB5 LED、PA2/PA3 I2C1。用户尚未给出三产品板卡差异，实际 App 接入前应逐个核实引脚、时钟和充电/显示外设。Boot 不初始化 App 的 I2C2。

## 构建与验证

X1 实施后，原有固件 preset 同时生成 Boot 和 X1，`APP_ENABLE_LOGGING` 独立控制 App；`ReleaseNoLog` 显式关闭两者。HostTests已增至14/14，包含启动、面板实包/响应事务、双版本识别/充电故障、I2C2失败恢复、真实G++构造函数链接以及MultiButton/按键/电源/低电量/复位ACK边界检查。当前产物检查与未验收事项见 [App/README.md](../App/README.md)。以下保留目录结构调整阶段的历史验证数据。

原有命令和 `build/<preset>/hc32f460_boot.{elf,hex,bin,map}` 产物路径保持不变：

```bash
cmake --preset Debug
cmake --build --preset Debug
cmake --preset Release
cmake --build --preset Release
cmake --preset ReleaseNoLog
cmake --build --preset ReleaseNoLog
cmake --preset HostTests
cmake --build --preset HostTests
ctest --preset HostTests
```

现有 5 项 Boot 回归保留，新增 3 项产品检查分别在没有 Boot 头文件搜索路径的情况下编译共用 LED 驱动，检查产品特性、32 KiB 地址契约、时间回绕、LED 初始化失败和重新初始化。它们验证代码/构建边界，不代替目标硬件验证。结构调整后的固件尚未在本次会话中刷写或复测硬件。

本次验证：HostTests **9/9** 通过（包含 GNU Arm 链接回归）；Debug、Release、ReleaseNoLog 构建通过，链接脚本分离前后 BIN 逐字节一致。X1、X1Pro、XBase 使用真实 startup/system/BSP 的临时最小入口均通过 `app::image` 试链接，确认向量表位于 `0x8000`、没有 ICG、SystemInit 按 App 的 VTOR 偏移编译。这些是链接验证，不代表 App 业务或硬件已完成。以下为实际 BIN 长度（含初始化数据的装载内容）：

| 配置 | BIN 字节数 | 距 32 KiB 上限余量 | 相对调整前 |
|---|---:|---:|---:|
| Debug | 26,824 | 5,944 | +48 字节 |
| Release | 23,192 | 9,576 | +48 字节 |
| ReleaseNoLog | 14,208 | 18,560 | +48 字节 |

三配置均确认 `FLASH_ORIGIN=0`、`FLASH_LENGTH=32768`、向量表起始地址为 0、ICG 地址 `0x400`/长度 `0x20`，ICG 内容和初始栈地址与调整前一致。协议解析/CRC、异常处理、地址范围实现保持逐字节一致；Boot 主循环、升级服务、I2C 和看门狗对照检查仅有包含路径和 API/配置名称变化。产品混用会被 CMake 拒绝。

`Tests/linker_layout_tests.py` 使用真实 HC32 startup 检查 Boot/App 向量表、MSP/复位向量、初始化数据、noinit 和镜像范围；同时验证 ICG 归属、错误起始地址/容量、代码越界以及初始化数据装载越界均被拒绝。HostTests 在找到 GNU Arm 工具链时注册该检查，固件 CI 必须执行它。

批量 LSP 诊断存在超时，未作为完整静态检查通过的依据；实际交叉编译、主机回归和产物检查均已完成。
