# X1 下一步计划：P3 实板验收 + P5 LED/SOC 补偿与参数保存

日期：2026-10-12。上位计划见 [X1 App 实施计划](x1_app_implementation_plan.md) §4 P3/P5，产品契约见 [x1_product_contract.md](x1_product_contract.md)。本文只安排下一轮工作，不修改已冻结的 P3 语义。

## 1. 工作区现状（本次核对）

- 分支 `feat/portability-node2-transport-boundary`，HEAD `cd2b38f`；近 5 个提交均为 X1 App 工作，分支名沿用旧移植阶段。
- 未提交改动仅有删除 `docs/superpowers/{plans,specs}/2026-08-25-phase2-transport-boundary*.md`（Phase 2 已关闭）。本计划不处理，由用户决定提交或恢复。
- `ctest --preset HostTests` 14/14 通过；`Debug` 构建为最新（`ninja: no work to do`）。未重新烧录，`x1-p3` 镜像仍未上板。
- 跨 MCU 的 [portability_plan.md](portability_plan.md) Phase 3–8 未启动，且与 X1 交付无依赖，本轮不排入。
- 进度：P3 软件完成、实板待验收；P4 基础 HIL 已过、电气待验收；**P5 未开始**；P6 依赖 P5。

## 2. 两条并行轨道

| 轨道 | 内容 | 前置条件 | 产出 |
|---|---|---|---|
| A：P3 实板验收 | 烧录当前 `x1-p3` Boot+App，按 §3 验收 | 用户授权刷写/上电刺激；V1.3 或 V1.5 板 + Linux 侧 | HIL 记录追加到 `x1_hil_test_version.md` 或新记录 |
| B：P5 软件 | §4–§6 | §4 决策冻结 | 代码、主机回归、三配置构建、文档 |

A 只依赖硬件，B 只依赖决策；B 的 Flash 时序测量（§6 B6）需要上板，可与 A 同一次上板完成。

## 3. 轨道 A：P3 实板验收清单

以当前 HEAD 干净重编译 Debug，记录 Boot/App BIN 哈希后再烧录；旧 HIL 哈希不复用。

1. 纯上电：PA0 连续 2 s 确认开机；10 s 未确认释放 PB3 进入仅充电驻留。
2. 软件/引脚复位：直接 Running，不等待按键。
3. 运行中 PA0 2 s → 0x02 关机位置 1；Linux 0x05=1 → 8 s 后 PB3 释放；不回 0x05 时 20 s 兜底。重复按键/确认不延长。
4. PA15 8 s 强制释放 PB3；外部供电时驻留、持续喂狗、不重新拉高 PB3。
5. 0x04=1：ACK 完整读出后复位进入 Boot 更新窗口；5 s 不读则取消，迟读不复位。
6. 双击 PA15 产生录制操作，0x02 完整读出后才确认。
7. 低电量：可用受控 BQ 数据或实际低电量；无法安全制造时标为未验收，不以主机回归代替。
8. 记录 `app_runtime_diagnostics.max_poll_gap_ms/max_work_ms`，以及 PB3、PA6 波形（逻辑分析仪）。

## 4. P5 决策（D3/D6/D7 已于 2026-10-12 由用户按推荐值确认）

旧实现依据：旧仓库 `771d4ea` 的 `Platform/HAL/HAL_Led.cpp`、`HAL_Power.cpp`（`Bat_Comp_*`、`Flash_*_CompOffset`、`Power_PowerOffMonitor`）、`mcu_config.h`。

| # | 问题 | 旧行为（已核对源码） | 推荐 |
|---|---|---|---|
| D1 | 补偿触发 | 充电中（`chargeStatus!=0`）原始 SOC 不变累计 20 min（按 500 ms 调用计数），则 `offset = 100 - raw` | 保留语义，改用实际毫秒差；仅有效且未陈旧的样本推进计时，样本无效/陈旧/拔出/SOC 变化均清零。“充电中”须包含 MP2762 终止态 `charge_state==3`：V1.5 终止后原始 SOC 常停在 98–99%，正是需补偿的场景（旧代码忽略 3） |
| D2 | 偏移上限 | `offset<10` 才保存，但 ≥10 时仍留在 RAM 并生效（缺陷） | `offset∈[0,9]` 才生效并保存；≥10 丢弃并记录诊断 |
| D3 | 偏移作用范围 | 0x01 面板 SOC、LED 分段、低电量 9% 均使用补偿后 `Percent=clamp(raw+offset,0,100)` | 面板与 LED 用补偿值；**低电量停录制/关机继续用 BQ 原始值**（更保守，P3 已按原始值验收）。**已确认** |
| D4 | 每次插电是否重新计算 | 拔出后清除“已更新”，下次充电可覆盖 | 保留：每次充电会话最多写一次，值不变不写 |
| D5 | 参数区 | `0x7A000`（与 Metadata A 重合，禁止） | 正式分配保留扇区 `[0x7E000,0x80000)` 为 `APP_PARAM_SECTOR`，写入 `firmware_layout.h` |
| D6 | 记录格式 | 单个 `uint32`，擦除态 `0xFFFFFFFF` 视为 0 | 16 字节追加记录：magic/版本、序号、offset、保留、CRC32（复用 `panel_crc32`）；扇区内顺序追加，满 512 条后擦除重写；取序号最大且 CRC 正确的记录，全无效时 offset=0。自研实现，不引入第三方库，理由见 §4.1。**已确认** |
| D7 | 写入时机 | 计算出时立即擦除+写入 | 仅在 Running/充电驻留、无未读响应、非看门狗脉冲期间写；擦除由追加满触发，罕见。实测后再定是否需要更严格窗口（§6 B6）。**已确认** |
| D8 | LED 有效电平 | PC13 电源灯、PH2 充电灯、PB5 功能灯，均高亮 | 沿用高有效；PC13/PH2 实物电平在轨道 A 上板时确认 |
| D9 | LED 规则与优先级 | 见 §5 | 按 §5 表，关机指示最高 |

D3、D6、D7 已确认，可实现；D1/D2/D4/D5/D8/D9 按推荐值实施，实板确认 D8 电平。

### 4.1 第三方磨损均衡/掉电保护库评估（2026-10-12）

约束：裸机、无堆、C 可编译；HC32F460 写粒度 32 bit、擦除单位 8 KiB；可用参数区只有 1 个扇区 `[0x7E000,0x80000)`；数据仅 1 个 offset，每次充电会话最多写一次。

| 候选 | 许可 | 适配点 | 不匹配点 |
|---|---|---|---|
| FlashDB KVDB（armink，与已用 EasyLogger 同作者） | Apache-2.0 | 无 malloc；`FDB_WRITE_GRAN` 支持 32；PRE_WRITE/PRE_DELETE 状态与 GC 掉电恢复 | `fdb_kvdb_init` 断言 `0 < FDB_GC_EMPTY_SEC_THRESHOLD(=1) < SECTOR_NUM`，即至少 2 个扇区（16 KiB）；需 FAL 移植层；功能远超 1 个字节的需求 |
| EasyFlash | MIT | 同作者旧版 | 已被 FlashDB 取代，同样需要备用扇区 |
| littlefs | BSD-3 | 可用静态缓冲 | 元数据成对块，实际需 ≥2 块且更多；代码体积和复杂度不相称 |
| Zephyr NVS / Pigweed pw_kvs | Apache-2.0 | 追加写+GC，掉电安全 | 依赖各自框架；同样需要始终空闲的 GC 扇区 |
| ST X-CUBE-EEPROM、Infineon emeeprom | 厂商许可 | 掉电恢复完整 | 绑定 STM32/PSoC，至少 2 页 |

结论：可靠的掉电保护都依赖“先写新扇区、再擦旧扇区”，至少需要 2 个擦除单位；在 1 个扇区内任何库都无法保护擦除窗口。因此 D6 采用自研追加记录（预计约 150 行）：

- 追加写入时掉电：CRC 拒绝半写记录，前一条有效记录仍在，属完全保护。
- 擦除窗口：仅在 512 条写满后发生，擦除后立即写当前值；此窗口内掉电则回到 offset=0 的安全默认值，符合产品契约中“擦除态/损坏恢复默认补偿”的规则。
- 若以后需要多个参数或擦除窗口也必须保护：改用 2 个扇区并引入 FlashDB KVDB。第二个扇区只能来自 Metadata B（与 Boot 的 A/B 元数据规划冲突），或把 `APP_FLASH_END` 下移 8 KiB（App 区有余量，当前 X1 Release 代码约 20.6 KiB，但需同步重刷 Boot；旧 Boot 升级时会擦除该扇区）。本轮不做。

## 5. LED 策略（由旧代码整理，修正已知缺陷）

优先级自上而下，第一个匹配的状态决定三灯输出：

| 条件 | 电源灯 PC13 | 充电灯 PH2 | 功能灯 PB5 |
|---|---|---|---|
| 关机流程（ShutdownWait/Delay、低电量关机） | 亮 | 灭 | 亮 |
| Off 驻留 / Starting 未确认 | 灭 | 按充电规则 | 灭 |
| 充电中，补偿后 SOC ≥100 | 灭 | 常亮 | 按录制规则 |
| 充电中，快充（`charge_state==2`） | 灭 | 300 ms 翻转 | 按录制规则 |
| 充电中，其他充电状态（`charge_state==1`） | 灭 | 1000 ms 翻转 | 按录制规则 |
| 充电器终止（`charge_state==3`，接电期间长期保持；2026-10-12 用户确认） | 灭 | 常亮 | 按录制规则 |
| 未充电，SOC ≥14 | 常亮 | 灭 | 按录制规则 |
| 未充电，10–13 | 1000 ms 翻转 | 灭 | 按录制规则 |
| 未充电，≤9（含 0%，修正旧代码排除 0%） | 300 ms 翻转 | 灭 | 按录制规则 |
| BQ 样本无效/陈旧 | 3000 ms 翻转 | 灭 | 按录制规则 |

录制规则：录制中 500 ms 翻转，否则灭；仅在 Linux 已出现过有效通信后启用（旧 `eg25_board` 门控）。

实现约束：LED 模块只读 `app_power_snapshot`、`app_battery_snapshot`、`app_recording_snapshot` 与补偿后 SOC；不修改录制、低电量计数或电源状态（旧代码在 LED 函数中停录制/计数关机的职责已由 P3 移入 `app_power`）。模式切换时立即写入目标电平并重置翻转时刻；约 100 ms 周期调度。

## 6. 轨道 B：P5 实施步骤

每步完成后运行 `ctest --preset HostTests`；涉及 BSP/布局时追加三配置构建。

**B1 布局与 BSP**
- `Common/Inc/firmware_layout.h`：新增 `APP_PARAM_SECTOR_BASE=BOOT_RESERVED_SECTOR_BASE` 等宏，`#error` 校验位于 App/Metadata 之外；同步 `boot_v2_memory_map.md`、产品契约参数区一节。
- 回归：Boot 擦除/下载范围检查拒绝 `0x7E000`（扩展现有 boot_core 范围用例）。
- `Drivers/BSP`：新增 X1 电源灯/充电灯 GPIO（`bsp_board_config.h` 加端口/引脚/有效电平，初始化为灭，C/C++ 均可用）。PB5 继续由 `bsp_status_led` 拥有，异常路径行为不变。
- **进度（2026-10-12，软件完成）**：`APP_PARAM_SECTOR_BASE/SIZE/END` 及编译期重叠检查已加入；Boot 回归改在 `boot_protocol_parser_tests` 的 Flash mock 中断言擦除/写入不越过 `APP_FLASH_END`，并新增下载到 `0x7E000` 返回 `ADDR_ERROR` 且不写 Flash。LED 驱动沿用 `bsp_keys`/`bsp_charger_gpio` 的 App 专用驱动写法，引脚在 `bsp_indicator_led.c` 内而非公共 `bsp_board_config.h`；`app_runtime` 在 PB5 之后初始化 PC13/PH2 为低（熄灭），失败按初始化失败处理。新增 `x1_indicator_led_tests`，HostTests 15/15；Debug/Release/ReleaseNoLog 的 Boot+App 构建通过，Boot Debug BIN 哈希仍为 `8e9fbd7e…`，未变。App 镜像已变化但构建 ID 仍为 `x1-p3`，B5 统一改为 `x1-p5` 并在 B7 更新 `App/README.md` 哈希。未烧录；PC13/PH2 实物电平待轨道 A 确认。

**B2 `app_led`（`App/Shared/{Inc,Src}/app_led.*`）**
- 纯函数 `app_led_select(输入快照) -> {三灯模式}` + 带时间的输出推进；主机测试覆盖 §5 每行、优先级切换、翻转回绕、0%/9%/10%/13%/14%/100% 边界。
- **进度（2026-10-12，待提交）**：`app_led_select` 新增 `charge_done`（state 3）常亮规则，`charging` 收窄为 state 1/2，与 `app_panel` 充电判定一致；翻转相位每次调用前移到一个周期内，避免 2³² ms 回绕后错相。`x1_led_tests` 覆盖 §5 各行、终止态、长时间运行；HostTests 16/16 通过。`check()` 替代 assert，因为 `app_led_level` 有副作用。未接入 app_runtime。

**B3 SOC 补偿（放在电池业务内，例如 `app_battery_compensation.*`）**
- 纯逻辑：输入（有效样本、原始 SOC、充电状态、now_ms），输出（当前 offset、是否需要保存）。充电状态 1/2/3 均计为充电：终止态 3 下 SOC 常停在 98–99%，若不计入，补偿永远不会把它推到 100%。
- 回归：20 min 刚好/差 1 ms、中途 SOC 变化、样本陈旧、拔出、offset≥10、时间回绕、同值不重复请求保存。
- `AppBatterySnapshot` 增加补偿后 SOC；按 D3 决定面板与 LED 消费哪个字段，`app_power` 低电量判断不改。

**B4 参数存储（`app_param_store.*`）**
- 仅操作 `APP_PARAM_SECTOR`，调用方自查边界；记录编码/扫描/追加/满后擦除；写后 `bsp_flash_read` 回读比对，失败不标记已保存。
- 主机测试用 Flash mock：擦除态、CRC 损坏、半写记录、扇区满、写/擦/回读失败、上电恢复取最新有效记录。

**B5 集成**
- 输入映射：`charging` = 充电器有效且 state 1/2，`charge_done` = 充电器有效且 state 3，`fast_charging` = state 2；`linux_seen` 需要 `app_panel` 提供只读访问器（至少收到一次有效帧），目前不存在。
- `app_runtime` 初始化加载参数（失败按 offset=0 继续，不阻止开机）；轮询中按 D7 条件执行一次保存，不在 ISR、不在看门狗高脉冲期间。
- 构建 ID 更新为 `x1-p5`，版本字符串 `X1-P5`。

**B6 目标测量（与轨道 A 同次上板）**
- 实测单次 16 字节编程与 8 KiB 扇区擦除的阻塞时间（`EFM_BUS_HOLD` 下取指暂停，I2C1 从机中断同样被延迟）。
- 在擦除期间发起 Linux 读写，确认无 NACK/数据错位，PA6 间隔仍在契约内。不满足时先收紧 D7 写入窗口；仍不满足再评估 RAM 执行，不先改平台方案。
- 验证 Boot 升级流程擦除/下载后 `0x7E000` 内容保持。

**B7 收尾**
- Debug/Release/ReleaseNoLog 的 Boot+App 构建、布局检查、HostTests；更新 `x1_app_implementation_plan.md` 进度表、`App/README.md` 哈希、`current_status.md`。

## 7. 完成判定

- P5 软件完成：D1–D9 已冻结，B1–B5 主机回归与三配置构建通过。
- P5 实板完成：B6 测量满足看门狗与通信预算，三灯电平/节奏与表一致，参数掉电后保留且升级不擦除。
- 之后进入 P6：升级闭环、完整 20 min 补偿窗口持续运行、P4 插拔/温度电气验收。
