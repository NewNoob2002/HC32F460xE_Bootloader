# App 产品维护入口

| 产品 | 独立配置 | OLED / LVGL |
| --- | --- | --- |
| X1 | [Products/X1/product.cmake](Products/X1/product.cmake) | 关闭 / 关闭 |
| X1Pro | [Products/X1Pro/product.cmake](Products/X1Pro/product.cmake) | 开启 / 开启 |
| XBase | [Products/XBase/product.cmake](Products/XBase/product.cmake) | 继承 X1Pro，按本产品需求增减 |

`app::X1`、`app::X1Pro`、`app::XBase` 是独立产品配置目标。X1 已接入 C++17 可烧录测试镜像 `hc32f460_app_x1`，支持真实 Linux 面板轮询、BQ 电池采样与运行时识别的双版本充电方案；X1Pro/XBase 暂无可执行入口。共用开关机、LED 和充电业务接入 `App/Shared/`；X1Pro/XBase 共用显示业务接入 `App/Shared/Display/`；产品专属代码放在各自 `Products/<产品>/`，共用 BSP 和协议只维护一份。

X1 采用与 Boot 相同的职责分离：

| 文件 | 职责 |
| --- | --- |
| `Products/X1/main.cpp` | 产品初始化入口与主循环编排 |
| `Shared/Inc/app_runtime.h` | 启动、轮询与故障公开契约 |
| `Shared/Src/app_runtime.cpp` | 接管电源/时钟/时基/看门狗、日志与周期心跳 |
| `Shared/Src/hc32f460_it.cpp` | App 异常入口与终止故障路径 |
| `Shared/Protocol/` | 无硬件依赖的 CRC32、字节解析与响应编码 |
| `Shared/Src/app_panel.cpp` | 面板通信与响应完整读取关联 |
| `Shared/Src/app_recording.cpp` | 录制状态、开始资格与版本化待通知操作 |
| `Shared/Src/app_keys.cpp`、`Libraries/MultiButton/` | 复用消抖/单双击事件，实际时间判定单次2 s/8 s长按 |
| `Shared/Src/app_power.cpp` | 冷启动确认、关机/驻留、低电量策略和完整复位ACK关联 |
| `Shared/Src/app_battery.cpp` | 电池有效性、硬件识别、双版本充电与温度保护 |
| `Drivers/BSP/bsp_i2c_master.*`、`bsp_charger_gpio.*`、`bsp_keys.*` | I2C2、V1.3充电GPIO及PA0/PA15按键输入 |
| `Libraries/Bq40z50`、`Libraries/Mp2762a` | 移除 Wire/Arduino 依赖后的有界芯片访问 |

启动要求 Boot 已完成交接；App 验证继承的 200 MHz 时钟，准备 SysTick/看门狗和日志后恢复中断。初始化失败保持中断关闭、保留电源输出并停止喂狗，供外部看门狗恢复。当前`x1-p5`保留MultiButton、按键及电源业务：除软件复位外的上电/引脚/看门狗复位都等待电源键2 s确认，10 s超时释放PB3（外部供电驻留中电源键2 s可重新开机）；运行时2 s请求关机、功能键8 s强制断电。`0x05=1`是Linux关机完成确认，8 s后断电且受最初20 s兜底约束；`0x04=1`等ACK完整读出才复位，5 s未读取消。SOC≤9%有效未充电时停录制，持续180 s请求关机。PB3释放后若仍有外部供电，继续充电/喂狗/通信并驻留，不自动复位或重新保持电源。详细边界见 [X1产品契约](../docs/x1_product_contract.md)。P5 已接入三灯、SOC 补偿与参数加载/保存；读取失败按 0 继续，保存由 Running/Off、未读响应和看门狗脉冲条件门控，失败保留 pending 并最早 3 s 后重试。HostTests 18/18、三配置/布局与改动文件的 clang-format/clang-tidy 检查通过；Flash 时序、Boot 参数隔离与擦除/Linux 事务重叠已按 [P5计划](../docs/x1_p5_plan.md) B6 实板验收，B7 产物见下文，LED 电平待实板确认。双版本充电按运行时可靠响应/NACK区分并锁定，错误不作为方案切换依据。

完整构建边界、链接地址与接入要求见 [工程结构](../docs/project_structure.md)。Boot 固定 32 KiB，App 从 `0x00008000` 开始。

App 使用 [Linker/HC32F460_app.ld](Linker/HC32F460_app.ld)，三个产品共用 456 KiB 的链接区域。实际 App 可执行 target 链接 `app::<产品>` 和 `app::image`，后者自动接入 App 链接脚本、平台源码和 VTOR 偏移；主机测试只链接产品配置。Boot 使用自己的链接脚本，App 禁止携带 ICG。

## 构建与验证

```bash
cmake --preset Debug
cmake --build --preset Debug --target hc32f460_app_x1
cmake --preset Release
cmake --build --preset Release
cmake --preset ReleaseNoLog
cmake --build --preset ReleaseNoLog
cmake --preset HostTests
cmake --build --preset HostTests
ctest --preset HostTests
python3 Tests/linker_layout_tests.py arm-none-eabi-gcc \
  build/Debug/hc32f460_app_x1.elf \
  build/Release/hc32f460_app_x1.elf \
  build/ReleaseNoLog/hc32f460_app_x1.elf
```

产物位于 `build/<preset>/hc32f460_app_x1.{elf,hex,bin,map}`，BIN 的编程基址是 `0x00008000`。旧阶段生成的 [Debug合并BIN](../build/Debug/hc32f460_x1_combined.bin) 不会自动随 App 构建更新；烧录时使用本轮独立App BIN，或用 `Tools/merge_bin.py` 重新生成到不存在的输出文件。Boot 产物路径保持不变。`APP_ENABLE_LOGGING` 独立于 `BOOT_ENABLE_LOGGING`；`ReleaseNoLog` 显式关闭两者。CI 已配置三种固件构建和 X1 产物收集，本地尚未执行远端 CI。

P5（B7 收尾，2026-10-18）：源码为 `87b80f0`（509-record D6），之后提交仅改文档。在空目录干净重编译 Debug/Release/ReleaseNoLog，三配置无编译告警；HostTests **18/18**，三个实际 X1 镜像通过上述布局检查。本次工具链为 PATH 上唯一的 GNU Tools for STM32 14.3.rel1（GCC 14.3.1，STM32CubeCLT 1.22.0，目录时间 2026-10-13 02:30），P3 哈希记录的 Arm GNU Toolchain 13.3.Rel1 已不在本机。下列哈希与 B6 实板部署的 Debug App `0ff3b7a1…`、Boot `8e9fbd7e…` 不同；生产源码相同，B6 未记录其工具链版本，差异推定来自工具链，未逐项比对。当前产物未烧录、未经实板验证，实板证据只对应 B6 记录中的镜像。

| 配置 | P5 App BIN字节数（含初始化数据） | SRAM静态布局含2 KiB栈预留 | Boot BIN字节数 |
| --- | ---: | ---: | ---: |
| Debug | 27,456 | 14,544 | 26,744 |
| Release | 23,336 | 6,608 | 23,304 |
| ReleaseNoLog | 16,436 | 5,048 | 14,300 |

P5 BIN SHA-256（GCC 14.3.1；App 基址`0x8000`，Boot 基址`0x0`）：

```text
App  Debug        cf5f8545985d34094064328219cdee75ab375b2009e73ae035536e26415c7c45
App  Release      88ed41e6f051ccbaebc9039dd277c064848f52d055d2d4c80af27481d1e9b057
App  ReleaseNoLog ce48c77c1103e50b80efa0d97f96cc74ddaa6c2cc2f256f82e082c5dc86c9307
Boot Debug        ba79317d7131f3e6db2008d20b600df2d4191d80add48dd45dadddc3961d76f0
Boot Release      1226827cedbda5a783bde65212ee75068ac87bd092b0f67c51b328dbe13860f7
Boot ReleaseNoLog e6163e52d768c4c400aa6a445da417c89283e874ee2cbaf09739733b8ed8587b
```

P3 实板修订（2026-10-18，见 [P3 HIL](../docs/x1_p3_hil.md)）：仅软件复位直接运行、其余复位等待电源键且 App 读后清除复位标志；外部供电驻留中电源键 2 s 重新开机；功能灯在本次开机首个 Linux 帧前常亮。上表 B7 App 哈希已被取代，Boot 不变。HostTests 18/18、三配置无告警、布局检查与改动文件 clang-format/clang-tidy 通过；Debug App 已在 V1.5 实板部署验证。

```text
App  Debug        15f475429c62df8541fba2ba4053006d317e7363853a136c1d1d1abc0e287bef  (27,608 B)
App  Release      4923693f58c4fb32d75e9079f0b93fd5788f7251b39c97a1abc4e145f13b7755  (23,480 B)
App  ReleaseNoLog 49afa32f928e6e7597386c9b31d8ec5d4914bc3bcb0674eee3efa0f7db855255  (16,576 B)
```

以下大小、哈希和软件验证为历史 P3 基线，不能用于校验当前 P5 产物。

P3软件验证：上述三配置及Boot构建通过，HostTests **14/14**；实际X1镜像通过向量、异常入口、无ICG、Flash装载范围和Boot依赖检查。混合C/C++调用、真实newlib构造函数表及startup调用顺序通过链接回归。新增回归使用真实MultiButton与BSP GPIO mock，覆盖电平/上拉、单击/双击、冷启动、单次长按、关机/确认重复、强制断电、低电量0%/无效陈旧/充电中断、复位ACK超时与时间回绕。新版实板验收尚未执行。

| 配置 | P3 BIN字节数（含初始化数据） | SRAM静态布局含2 KiB栈预留 |
| --- | ---: | ---: |
| Debug | 24,456 | 14,496 |
| Release | 20,848 | 6,560 |
| ReleaseNoLog | 14,160 | 4,992 |

P3 App BIN SHA-256（基址`0x8000`；分节点提交前以 Arm GNU Toolchain 13.3.Rel1 干净重编译复核）：

```text
Debug        f41edf8ecdbd81f61374ac2a236dfb2279a9ef05ba4965abaf2cd17dba6048ba
Release      2fed848af935ac37f97d6405f807c5e1a807b77719f9af870bec05adaa9aeac7
ReleaseNoLog 531af698e387145059a15109fa9a969a4be03fefdf510d3987a922f0414a4acb
```

本次 Boot Debug BIN SHA-256 为`8e9fbd7ebbb5183442a05f302700ba0879d038391f94a67a2f9a11879fe665eb`，与旧HIL基线不同；本轮Boot与App均仅完成软件构建/回归，未烧录。旧HIL中的`68e2d58c75b04a1357109def42ad586586626210e76f913946592bd70e920a9c`只标识当时验证的Boot，不能用于校验当前产物。

无日志镜像无 RTT/EasyLogger 或 malloc 符号；日志镜像复用既有 newlib stdio 依赖，没有新增业务堆分配。表中栈大小是链接预留，不是实测峰值。P3有界LSP探测14个文件全部超时，未作为静态检查通过的证据。

以下为旧`x1-hil1`镜像的实板证据，不证明新P3镜像：

已在 HC32F460JEUA X1 V1.3 烧录 Debug 并完成独立 Boot/App verifybin。自动识别 V1.3，实际 BQ 样本有效，约 7 分钟的两次自由运行快照有 684 条有效 Linux 帧和 501 次完整支持消息响应读取，错误/恢复计数为 0。MP2762 路径已补充 [V1.5实板HIL](../docs/x1_v15_hil.md)：新板已有同一BIN，独立verifybin通过，自动识别约3ms完成，配置写后回读与快充状态读取通过；非显式暂停窗口约10分钟有1251次完整响应读取，CRC/长度/BQ/MP通信及恢复计数均为0。显式调试暂停阶段的恢复和一次PIN复位现象单独保留，未混为正常运行通过。电气充电保护、Type-C插拔与PA6精确波形仍待验收。失败尝试、哈希、快照与剩余进度见 [测试基线记录](../docs/x1_hil_test_version.md)。

```sh
JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/x1_flash_debug.jlink
JLinkExe -NoGui 1 -ExitOnError 1 -CommandFile Tools/jlink/x1_verify_run_debug.jlink
```

烧录前检查目标与产物哈希并备份 Flash；停止占用探针的 GDB Server，按 [J-Link 说明](../Tools/jlink/README.md) 执行。
