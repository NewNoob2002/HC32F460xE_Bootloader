# X1 B6 Flash 实板测量与参数保持记录

日期：2026-10-13 UTC。B5 提交为 `5b2f86e`。用户要求“review and commit B5, then do B6”；B5 review 无阻塞问题，HostTests 18/18、Debug/Release/ReleaseNoLog 构建、实际 App 布局及改动文件的 clang-format/Host+ARM clang-tidy 检查通过。

**状态：B6 完成（2026-10-13）。** 真实 Flash BSP 单次计量、回读验证、Boot 擦除/下载处理路径参数隔离、一次正常 D7 满扇区回收均已完成；擦除/Linux 事务重叠由 MCU 侧 NVIC pending 探针直接证明（42 次重叠，含 13 次写入），Linux 1100 次擦除期间 613 次传输全部成功、受支持请求的响应 CRC 全部正确，见文末“擦除与 Linux 事务重叠验收”。用户已确认外部看门狗超时为 60 s，取消 PA6 波形测量验收。D7 窗口未修改。

## 目标与部署

- 当前连接与此前 V1.5 HIL 的探针和整片 Flash 备份一致；MCU 使用已确认的 HC32F460JEUA 对应 `HC32F460xE` J-Link 系列入口。
- J-Link V9.76a，SWD；编程 100 kHz，备份/诊断 4000 kHz。VTref 约 3.407–3.417 V，CPUID `0x410FC241`。
- 编程前已保存 512 KiB Flash。备份 SHA-256 为 `38d371d075b0e7a549177866730799c42953df781b74832f871b15cafecfb039`，与此前 V1.5 记录完全一致；原 App 标识为 `x1-hil1`。ICG 与本轮 Boot 一致。
- 原 `[0x7E000,0x80000)` 全为 `0xFF`，没有有效补偿记录。该 8192 字节备份 SHA-256 为 `7d2c7ac4888bfd75cd5f56e8d61f69595121183afc81556c876732fd3782c62f`。
- 按既有 X1 脚本部署并在独立 reset/halt 会话中执行 Boot/App `verifybin`；均通过。未修改保护设置，未 mass erase。

| 镜像 | 基址 | SHA-256 |
| --- | --- | --- |
| Boot Debug | `0x00000000` | `8e9fbd7ebbb5183442a05f302700ba0879d038391f94a67a2f9a11879fe665eb` |
| X1 App Debug，`x1-p5` | `0x00008000` | `4a6e383b3122a360b03dd4ecfdf1bac2e47f0808c4d46254e8134857498732b0` |

部署后 RTT 明确输出 `id=x1-p5 product=X1 ready clock=200000000 Hz` 和 `hardware=V1.5 mode=2`。初始自由运行样本为 115,388 ms，发布/确认/完整读取响应均为 180；BQ、MP、保存失败与 I2C 恢复计数为 0，Flash 尾部与部署前一致。

## 16 字节编程与 8 KiB 擦除

方法：使用准确 B5 ELF，在 `app_runtime_poll` 的安全窗口检查位置暂停；确认 Running、无未读响应、看门狗不在高脉冲。用独立 GDB batch 会话调用本轮镜像中的真实 `bsp_flash_write` 和 `bsp_flash_erase_sector`，以 Cortex-M4 DWT CYCCNT 的前后差计量。未改写业务固件源码。

写入只限参数扇区：一条版本 1、序号 1、offset 0 的有效记录。编程源在对齐 RAM 中；写后四个字回读与预期完全相同。随后通过真实 BSP 擦除 sector 63，完整 8192 字节回读与原备份逐字节相同；临时 scratch RAM 与 trace 控制值已恢复。

| 操作 | 返回码 | cycles | 按配置 200 MHz 换算 |
| --- | ---: | ---: | ---: |
| 16 字节编程 | 0 | 39,338 | 196.69 µs |
| 8192 字节扇区擦除 | 0 | 3,610,016 | 18.05008 ms |

这是每项一次的调用计量，包含调用及可能的 ISR 开销；换算使用配置的时钟频率。DWT 计量排除了两次 GDB 操作之间的停机时间。没有独立外部时基或波形交叉验证，不能当作最坏值、精确物理时钟校准，或正常主循环下的 Linux/PA6 并发验收。Flash 停顿时 1 ms SysTick 可能丢失累计 tick，因此不能用 `max_work_ms` 的普通快照替代该计量。

原始证据：`debug_artifacts/x1_b6/plain_flash_timing_batch.log`、`isolated_flash_timing.json`、`after_isolated_params.bin`。

## PA6 验收要求调整

用户在本轮明确确认 TPL5010 外部看门狗超时为 **60 s**，因时间裕量充足，**取消 PA6 波形测量验收**。本次单扇区擦除样本约 18.05 ms，作为时序裕量依据；不将单次样本推导为最坏耗时，也不把取消的测量记为实测通过。固件仍采用每 3000 ms 输出 1 ms DONE 高脉冲的既有策略。PA6 波形设备不再是 B6 前置条件；Linux 事务与擦除区间重叠的通信验收单独保留。

## Boot 擦除/下载路径的参数保持

使用本轮 Boot ELF，从 Boot `main` 硬件初始化后的暂停点调用实际 `encode_response` 的 ERASE/DOWNLOAD 处理分支。RAM 中放入精确的 `boot_protocol_frame_t`（528 字节）；此方式绕过 I2C parser 和 ACK 传输，验证的是 Boot 的真实 Flash 处理路径，不是 Linux uploader 全链路。

1. 在参数扇区写入同一条 offset 0 有效记录。整个 seeded sector SHA-256 为 `093d66743acb9399f6193e75450ec13d34564148a2a69b63660ae5bf96ed20c3`。
2. ERASE 指向 `APP_FLASH_BASE=0x8000`，执行既有 `erase_application()` 范围。响应长度 21、ACK 0、`erase_commands=1`、`flash_errors=0`；App 首尾检查字均为 `0xFFFFFFFF`。参数扇区完整回读与 seeded sector 相同。
3. DOWNLOAD 指向 `0x8000`，写入已验证 B5 镜像的前 16 字节。响应长度 21、ACK 0、`programmed_bytes=16`、`flash_errors=0`；App 数据回读相同，参数扇区保持相同。
4. DOWNLOAD 指向 `0x7E000`，返回 ACK 4（`ADDR_ERROR`）；编程字节数仍为 16、Flash 错误为 0，参数扇区仍相同。
5. 恢复原参数扇区的全 `0xFF` 内容，重新部署完整 B5 App。Boot、App、原参数扇区均通过 `verifybin`；整个 `[0x7A000,0x80000)` 与实验前备份逐字节相同。

该样本证明 Boot 擦除/下载实现的范围隔离在目标上生效。完整镜像经 Linux uploader 更新、ACK/跳转闭环仍属于后续整体升级验收。

原始证据：`debug_artifacts/x1_b6/boot_parameter_preservation.log`、`boot_parameter_preservation.json`、各阶段 `boot_params_*.bin`、`restore_b5_after_boot_test.log`。

## 先前恢复运行的正常轮询样本

清除测试断点和 debug comparators 后恢复核心运行。最终运行中读取前后的 DHCSR 均为 `0x01010001`（S_HALT=0），VTOR 为 `0x8000`，CFSR/HFSR 均为 0。GDB server 已停止。

| 观测量 | 实际值 |
| --- | ---: |
| 固件运行时间 | 148,652 ms |
| 主循环次数 | 9,157,233 |
| 发布 / 确认 / 完整响应读取 | 244 / 244 / 244 |
| I2C 恢复 active / busy / init failure | 0 / 0 / 0 |
| RX overflow / arbitration / response busy / empty TX | 0 / 0 / 0 / 0 |
| BQ / MP / 参数保存失败 | 0 / 0 / 0 |
| 原始 SOC / 电压 / 温度 | 48% / 7405 mV / 3092（0.1 K） |
| 状态 / 充电状态 / offset | Running / 2（快充） / 0 |
| 普通轮询最长间隔 / 工作时间 | 10 / 10 ms |

`nack_events=244` 与完整读取数一致，是 MCU 记录的读事务末尾 NACK 事件，不能据此判断 Linux 端出现 244 次地址 NACK，也不能据此证明没有地址 NACK。没有取得 Linux 端 CRC/业务校验日志。

上述普通轮询样本在实验后的恢复运行阶段取得。实验中的 debugger pause/reset 不计入该样本，不将暂停前后的计数拼成连续运行通过。

## 保留的调试问题与未完成项

- MCP 初次 GDB 脚本无法执行 Python（当前 Arm GDB 不含 Python 支持）；后续 MCP 的 async inferior 状态也阻止 plain script 修改设置。两次脚本均未进入参数写入阶段。之后通过独立、有限时长的 plain GDB batch 执行并取得上述结果。
- 调试中多次 halt/resume、Boot service 测试和镜像恢复打断了正常时间线；不能据这些区间声明连续 Linux 通信或精确 watchdog 波形通过。最终已恢复并确认运行。
- 用户随后确认已连接 EG25-G USB，设备新枚举为 `2c7c:0125`、产品 EG25-G；`ttyUSB1..ttyUSB5` 分别为 USB interface 00..04。8 s 被动采集只在 `/dev/ttyUSB5`（interface 04）取得 690 字节可读控制台输出，包括 `X1 login:`、`mdm-perf 202003131021 X1 /dev/ttyGS1`。已确认 Linux console 稳定路径为 `/dev/serial/by-id/usb-Quectel_EG25-G-if04-port0`；采集保持原 USB baud 元数据 9600，并未猜测物理 UART。没有发送用户名/密码或改变 modem 配置，当时需要已认证 shell。原始证据 `ttyUSB5_passive.bin`、`serial_passive_summary.json`。`ttyUSB0` 为独立 CH340，`ttyACM0` 用途未确认；PC I2C adapter 未扫描/写入。
- 用户确认没有 capture equipment；未取得 PA6/SCL/SDA 同步 trace。PA6 波形测量要求随后由用户取消。
- **已由文末重叠验收关闭**：同步确定 Linux 事务与真实擦除区间的重叠，确认该区间的 NACK/重试与字节对齐结果。正常 D7 满后擦除已实际执行，下面 SSH 抓取覆盖保存前后，但不能单凭宽时间标记关闭“擦除期间发起读写”的要求。实测不满足时才据证据收紧 D7，必要时再评估 RAM 执行。

## Linux 基线、追加满准备与 D6 修订发现过程

用户登录并释放 picocom 后，串口确认 `uid=0(root)`，Linux 为 `3.18.44 armv7l`，`Surveyor` 使用 `/dev/i2c-2`。未停止/重启 Linux app，也未启动额外 I2C master。创建独立 `b6_flash` ftrace instance（512 KiB buffer），只记录 adapter 2 与地址 `0x11` 的读写/回复以及 adapter 2 的 transfer result，保留已有 global tracing 配置。

3 s 正常基线捕获 6 个真实请求与 6 个响应（ID 2/3），共 12 帧；逐帧长度、头字段及 CRC32 全部正确，12 次 transfer result 均为 `n=1 ret=1`。这是未擦除时的 Linux 端基线，不能报告为擦除并发通过。证据：`linux_i2c_baseline.log`、`linux_baseline_crc.json`。

为准备 normal-runtime 擦除，曾通过真实 BSP 批量写入 8192 字节的 512-record fixture。调用返回 `-1`，完整回读仅有前 510 条 CRC 有效，最后 32 字节仍为 FF。保留失败后立即擦除恢复原参数，并回读验证。该批量失败的具体原因尚未证明；它既不同于原 16 字节编程样本，也未进入 normal runtime 保存触发阶段。证据：`runtime_prepare_full_sector.log`、`runtime_seed_failure.json`、`runtime_restored_after_seed_failure.bin`。

检查 SDK 同时发现 `Drivers/hc32f4xx_ll_drivers/inc/hc32_ll_efm.h` 定义 `EFM_SWAP_ADDR=0x0007FFDC`、`EFM_SWAP_DATA=0xFFFF4321`。512 条记录覆盖该控制字；因此提出排除控制尾部、仅容纳 509 条的 revision（最后记录结束于 `0x7FFD0`）。助手曾提前修改并部署此 revision；这超出 B6 的测量范围，也改变已确认 D6 的“满 512 条”要求，已停止进一步实验并撤回生产源码/镜像，不把它当作已批准的产品变更。

该提案已形成可审查 patch：`debug_artifacts/x1_b6/proposed_509_record_revision.patch`，相关回归在原代码上失败，提案上 HostTests 18/18、三配置构建与 clang-format/Host+ARM clang-tidy 通过。仅写 8144 字节的 509-record fixture 时 BSP 返回 0，编程源及 Flash 回读逐字节相同，尾部 48 字节为 FF。此结果没有证明原批量失败原因，不能据此把 D6 从 512 静默改为 509。

随后 Linux console 回到 `X1 login:`；没有写入 runtime `save_pending` 触发，正常主循环擦除/并发测试没有执行。独立 ftrace instance 已在基线结束时关闭，尚需重新登录清理该 instance；global tracing 未修改。用户确认没有 capture equipment，PA6/SCL/SDA 波形仍没有证据。

## 撤回提案后的恢复结果

原 all-FF 参数扇区已恢复并逐字节核对；生产源码与 B5 `5b2f86e` 一致。Debug App 重新构建 SHA-256 恢复为 `4a6e383b3122a360b03dd4ecfdf1bac2e47f0808c4d46254e8134857498732b0`，三配置与 HostTests 18/18 通过。重新编程时一次 J-Link loader 失败及后续 startup HardFault 均保留，不算通过。

复查发现 FPB comparator 0 仍为 `0x4000B505`，对应地址 `0xB504`；该地址是临时镜像 reset entry，在恢复的 B5 中落在 `0xB502` 开始的 32-bit 指令内部。清除全部 debug comparators 后从 Boot vector `0` 复位运行，最终普通运行快照为 27,009 ms、1,658,620 次轮询，状态 Running，offset 0、save_pending 0，响应发布/确认 5/5；DHCSR 前后均为 `0x01010001`，CFSR/HFSR 均为 0，整个 `[0x7A000,0x80000)` 与原备份相同。没有更改保护等级、执行 mass erase 或保留新的 D6 产品实现。

恢复证据：`restore_original_b5_contract.log`（失败）、`restore_original_b5_exclusive.log`（Boot/App/参数 verifybin 成功）、`contract_final_running.log`、`contract_restored_state.json`。当时提案 artifact 与失败 evidence 独立保留，D6 尚待用户决定。

## 用户批准 509-record 修订后的准备

用户在本轮明确选择“Approve 509 records”。现已重新应用四个源码/测试文件的最小 revision，同步产品契约和 D6；记录版本/字段、扇区擦除大小与 D7 不变。HostTests 18/18、Debug/Release/ReleaseNoLog 构建、clang-format 与 Host/ARM clang-tidy `--warnings-as-errors=*` 均通过。Debug App SHA-256 为 `0ff3b7a1ee8e86502119f456f21b86cfb64baee60bdbd812a3b0828f3785d154`，Boot SHA-256 不变。

用户重新登录后确认 `uid=0(root)`、Linux uptime 552.87 s，旧 `b6_flash` instance 已不存在，证明之前的 Linux trace 状态没有保留。为避免镜像装载时复位 GPIO，尝试 core-only VECTRESET；J-Link RAM loader timeout，未完成部署，保留 `flash_approved_509_core_only.log`，不作为通过。随后使用 normal reset/独占编程并显式清除 FPB comparators、选择 Boot vector `0`，Boot/App/原参数三项 verifybin 成功。正常运行快照为 1136 ms、60,917 次轮询、Running、offset 0；CFSR/HFSR 为 0、核心未 halt。证据：`flash_approved_509_normal.log`、`approved_509_runtime.bin`。恢复使用标准 reset，core-only 不用于后续部署。

通过准确新 ELF 的实际 BSP 成功写入 8144 字节，`runtime_seed_rc=0`；完整参数回读与源 509 条 CRC-valid、offset 1、递增序号 fixture 相同，尾部 48 字节为 FF。RAM offset 保持 0、save_pending 0，没有设置 runtime 触发。GDB batch detach、server 停止并清除断点后立即显式 resume，避免清 comparator 操作留下 core halted。证据：`approved_runtime_prepare_509.log`、`runtime_509_seeded_params.bin`、`runtime_509_source_after.bin`。

部署后控制台不再返回 completion marker，后续输出为 `X1 login:`。检查主机 USB tty 实际属性发现 `HUPCL/ECHO/ICANON` 全部开启；已配置 raw 并清除 HUPCL，串口 helper 退出时保留该位清除（与 picocom `--noreset` 相同），避免 close 降低 DTR 导致 hangup，也避免本地 echo 将 getty banner 反射回 Linux。helper 收到已缓冲的 login/password 提示时不发送诊断命令。尚未以新登录验证持续 session，因此不能把所有先前 console 断开或 Linux 重启都归因于 HUPCL。

**准备后的状态**：批准后的 509-record App 正常运行，参数扇区暂存完整 offset-1 fixture。后续通过准确 ELF 的变量符号读取确认 RAM offset 已为 1、save_pending=false、save_attempted=false、save_failures=0；因此原“RAM 仍为 0”只描述写 fixture 时的暂停点，不能当作后续触发前状态。正常 D7 擦除测试尚未开始，后续应把 RAM offset 改为 0、标记 pending 后再让正常 gate 执行。原 all-FF 参数备份保留，测试后将恢复并核对原内容。需要再次登录 Linux 才能创建新 trace、触发正常 runtime 保存并检查 Linux 并发；PA6 波形仍无设备。

再次手工登录后，两次连续串口 helper 调用均成功，分别取得 uptime 1644.71/1644.77 s 与 `Surveyor -d 7`（PID 1286）。随后符号检查、GDB server 停止/清断点与 resume 之后，创建 trace 前的控制台已有 `X1 login:`，helper 因提示检测未发送命令；没有创建 trace 或运行 trigger。此证据只证明连续 open/close 可短暂保持会话，不能证明 HUPCL 是后续断开的根因。现已启动一个仅持有 fd、不读取串口的本地进程，避免命令之间 last-close，保存 `console_hold.pid`；完成串口工作后需停止。用户询问避免重复认证的方法，既有 process 证据显示 Linux 运行 `sshd`，可在有可达 IP 后配置 SSH 公钥；主机有 adb，但 `adb devices -l` 当前为空，不能据 `adbd` process 宣称 ADB shell 可用。用户尚未确认该次登录恢复，B6 仍阻塞，fixture 保持未触发。

## SSH 接入与正常 D7 回收样本

用户已配置 SSH 公钥并将主机连接设备 WiFi，明确指定 `root@192.168.10.12`。`BatchMode=yes` 非交互认证成功，不需要串口保存凭据；目标为 Linux `3.18.44`，`Surveyor -d 7` PID 1286，持有 `/dev/i2c-2`，adapter 名称 `MSM-I2C-v2-adapter`。记录 `linux_ssh_identity.log` 与 Linux boot ID，未停止应用、启动额外 master 或更改 SSH/网络配置。

先以匹配 artifact 验证 Boot/App，通过完整尾部回读确认 metadata 未改、509 条 fixture 有效且后 48 字节为 FF。此镜像检查在抓取前完成，debug interruption 不属于抓取样本。RAM 实际 offset 1，save_pending/save_attempted 均为 false、save_failures=0。随后关闭 GDB server、显式 resume，创建独立 `b6_flash_ssh` ftrace instance（512 KiB，仅 adapter 2 与地址 `0x11`，全局 tracing 不改）。

在 running core 上通过有限 SWD 字节写设置 `session_done=true`、offset 0、`save_pending=true`，没有在该触发脚本发出 halt/reset、设置 breakpoint 或直接调用 Flash 函数。真实 `app_runtime_poll` 的 D7 条件选择窗口并调用保存。触发脚本开始/结束 DHCSR 均为 `0x01010001`，CFSR/HFSR 为 0。此为人为 pending stimulus，不证明真实 20 min SOC 稳定生成条件。

最初 50 ms 后的 Flash dump 仍为 full fixture，而随后 RAM dump 已显示 pending 清除；两份顺序 dump 不能作为同一时刻的原子快照，初次断言失败保留在 `ssh_runtime_save.json`。后续独立完整扇区回读确认只剩一条版本 1、序号 1、offset 0、CRC 正确的记录，后续 8176 字节全 FF，RAM offset 0、pending=false、save_attempted=true、save_failures=0。证明 full-sector 回收、追加与运行时成功确认已完成；结果见 `ssh_runtime_save_final.json`。

| Linux 抓取观测 | 实际结果 |
| --- | ---: |
| 开始 / 结束 kernel timestamp | 417.176531 / 500.424150 s |
| 抓取跨度 | 83.247619 s |
| request writes / response replies | 165 / 164 |
| 全部捕获数据帧长度 / CRC | 329 / 329 正确 |
| transfer result | 329 次 `n=1 ret=1` |
| buffer overrun / commit overrun | 0 / 0 |
| 最长捕获 transfer | 5.242 ms |

末尾多一条 request 是 capture 截止边界，不把未捕获的后续 response 算作失败或通过。抓取中没有 Linux 返回的传输错误、无效 CRC 或长度，但 `i2c_result` 只记录最终 adapter 返回，不能证明硬件从未 NACK 或驱动内部没有重试。before/after marker 为 419.494668 / 495.418885 s，未记录 Flash 实际开始/结束的同步 timestamp；不能证明某条 Linux transaction 恰好落入约 18 ms 擦除区间。PA6 波形测量要求已按用户决定取消。

原始证据：`linux_ssh_runtime_trace.log`（826 entries）、`linux_ssh_runtime_crc.json`、可运行标准库解析器 `analyze_ssh_trace.py`、`ssh_runtime_trigger.log`、`ssh_after_capture.log`、各完整 RAM/参数 dump。Linux boot ID 从接入、抓取到清理保持一致；SSH 后续仍成功，Surveyor PID 1286 未变。

## 本轮恢复与最终状态

在抓取停止后，通过真实 BSP 单次擦除 sector 63，返回 0；完整 8192 字节与不可变原 `before_params.bin` 一致。恢复 offset 0、timing/session_done/pending=false 及未尝试状态，取消注入的控制状态；此恢复中的 debugger interruption 不作为正常擦除样本。GDB server 停止、debug comparator 已停用并立即 resume。移除本轮创建的 `b6_flash_ssh` instance，停止旧串口 fd holder，保留原始日志/备份。

最终运行回读：262,726 ms、16,347,023 次轮询，Running、offset 0、pending=false、session_done=false、save_failures=0，发布/确认响应 517/517，整个 `[0x7A000,0x80000)` 与原备份逐字节相同。DHCSR 前后为 `0x01010001`，VTOR `0x8000`，CFSR/HFSR 0；这是一段恢复后的样本，不与抓取计数拼接为连续 MCU 运行。批准的 509-record App 保持部署；D7 未改。证据：`ssh_restore_original_params.log`、`linux_ssh_cleanup.log`、`ssh_final_state.json`、`ssh_final_snapshot.log`。

Debug/Release/ReleaseNoLog 三个实际 App ELF 的向量、Flash load 范围、无 ICG/Boot 依赖检查也通过。SSH 认证阻塞已解除，PA6 波形测量要求已由用户取消；B6 仅剩擦除/事务精确重叠的通信验收，LED/完整产品验收仍按上位计划进行。

可恢复的全 Flash、参数备份、preflight、脚本与原始结果均保留在忽略目录 `debug_artifacts/x1_b6/`。

## 擦除与 Linux 事务重叠验收（2026-10-13）

前次会话曾尝试主机侧按时钟同步预测 Linux 写入时刻并触发擦除（`overlap_*` 文件，attempt 1–4），未形成结论；其遗留的参数扇区为一条序号 1、offset 0 的有效记录（与 `overlap_4_params.bin` 相同），本轮测试擦除后已按原 all-FF 备份恢复。

**方法**：不依赖跨设备时钟同步，改为 MCU 侧直接判定。在独立 git worktree 中对已提交 `87b80f0` 加 scratch 探针（`b6probe_harness.patch`，ID `x1-b6probe`，App SHA-256 `570b7db3…`，未提交到仓库），位置为 `app_runtime_poll` 中真实 D7 门控之后（Running/Off、无未读响应、看门狗非高脉冲）。探针按 RAM 可调周期调用真实 `bsp_flash_erase_sector(63)`（扇区本已为空），擦除前 `__disable_irq` 并记录 I2C1 EEI/RXI/TEI（INT005/006/004）NVIC pending，擦除后再读取；仅擦除期间新出现的 pending 计为重叠。`EFM_BUS_HOLD` 下 CPU 取指本已停顿、ISR 无法执行，屏蔽中断只把 ISR 推迟到擦除返回后的数个指令。Linux 侧经 SSH 用独立 ftrace instance 只记录 adapter 2 / 地址 `0x11`。探针使能、周期与上限均为运行中 SWD 写 RAM，未 halt。

| 轮次 | 周期 | 擦除次数 | 擦除耗时 | 重叠（任意 / 地址 EEI / RX） | Linux 传输 / 失败 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 211 ms | 400 | 18.039–18.053 ms | 1 / 1 / 1 | 349 / 0 |
| 2 | 100 ms | 400（含抓取后） | 同上 | 26 / 26 / 0 | 175 / 0 |
| 3 | 25 ms | 300 | 同上 | 15 / 14 / 12 | 89 / 0 |

合计 1100 次擦除、0 次 BSP 失败、42 次擦除窗口内 I2C 地址匹配（13 次含 RX）。三轮 MCU 计数增量：RX overflow、arbitration、response busy、恢复 active/busy/init failure、parser CRC/长度/超时错误均为 0；`rx_transactions` 与 parser `valid_frames` 相等（174/87/45）；`max_poll_gap_ms`/`max_work_ms` 为 2/2（取样时探针已停止，擦除轮次因 SysTick 停顿不能由此量测）。DHCSR 始终 `0x01010001`，CFSR/HFSR 为 0。ftrace overrun 0。

**观测**：
- 擦除期间开始的事务由从机时钟拉伸到擦除结束：读最长 19.92 ms、写最长 22.81 ms，Linux `i2c_result` 全部 `n=1 ret=1`，没有 NACK 返回。重叠写入在 MCU 端全部解析为 CRC 正确帧。
- 受支持请求（ID 1/2/3）在三轮中全部得到 CRC 正确的响应（含被拉伸的写之后）；第 1 轮周期 211 ms 时擦除被 D7 “无未读响应”门控锁相到响应读取之后，仅取得 1 次重叠，因此追加 100/25 ms 两轮扫描相位以覆盖写入。
- 所有被拉伸的读均为“无待发响应”的空读（全 `0xFF`）：D7 门控使擦除不会与待读响应重叠，这正是设计意图。
- 分析脚本将 112 字节 ID 6 写入标为无效，原因是 ftrace 只打印前 64 字节，不是线路错误。

**独立发现（非擦除问题）**：Linux 启动后 Surveyor 周期性发送 ID 6、8–13 请求（每约 15 s 各一次），X1 仅实现 1–5，不发布响应，随后 Linux 读取得到 26 字节全 `0xFF`（MCU `empty_tx_reads` 计数）。三轮中未支持 ID 的空读与请求 7/7 一一对应，与是否擦除无关。之前 uptime 417 s 的抓取未出现这些 ID。是否需要实现或显式拒绝这些消息属于协议范围，待用户确认。

**限制**：Linux `i2c_result` 只反映 adapter 最终结果，不能证明驱动内部无重试；没有 SCL/SDA 波形。探针擦除未经过 `app_param_store_save` 的扫描/追加，但使用相同 BSP 擦除函数与相同 D7 门控位置。

**副作用与恢复**：两次以 normal reset 烧录（探针镜像、恢复镜像）均使 MCU 复位并经 PB3 导致 Linux 重启（boot ID `69a2dcc4…`、`e91c3ea8…`），Surveyor 自动恢复。扇区 63 本轮新增约 1100 次擦除。最终重新部署批准的 509-record App（`0ff3b7a1…`），Boot/App/原 all-FF 参数扇区三项 verifybin 成功；最终运行 DHCSR `0x01010001`、VTOR `0x8000`、CFSR/HFSR 0，参数扇区与 `before_params.bin` 相同；ftrace instance 已移除。

证据：`b6probe_harness.patch`、`b6probe_app.{bin,elf}`、`flash_b6probe.log`、`b6probe{,2,3}_run.sh`、`b6probe{,2,3}_{pre,post}_*.bin`、`b6probe{,2,3}_linux_trace.log`、`b6probe{,2,3}_result.json`、`analyze_b6probe{,2,3}.py`、`restore_after_b6probe.log`、`probe_final_snapshot.log`、`b6probe_linux_final.log`。
