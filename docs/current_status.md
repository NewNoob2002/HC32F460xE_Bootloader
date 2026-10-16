# Current implementation status

本次结构调整：Boot 集中到 `Boot/`，协议位于 `Common/Protocol/`，板级驱动/时基/日志端口供 Boot/App 共用；三个 App 产品提供独立配置入口；X1 已接入 C++17 可烧录测试镜像，旧 `x1-hil1` 已完成V1.3/V1.5双版本充电与Linux轮询基础HIL，见 [V1.5记录](x1_v15_hil.md) 和 [测试基线HIL](x1_hil_test_version.md)。本轮 `x1-p5` 保留MultiButton与P3按键/电源/低电量/复位ACK业务，并完成三灯与SOC补偿参数保存集成；HostTests 18/18、三配置Boot/App构建、布局及改动文件的clang-format/clang-tidy检查通过。B5提交 `5b2f86e` 已部署并独立verifybin到V1.5目标；[B6实板记录](x1_b6_hil.md) 给出单次编程约196.69 µs、擦除约18.05008 ms和Boot参数隔离证据，参数/Flash尾部恢复且最终运行正常。B6擦除与Linux事务重叠已由MCU侧探针验收（1100次擦除、42次重叠、Linux传输全部成功）；**P3按键/开关机、LED电平及产品电气验收仍待完成**。D6 已由用户批准从 512 修订为 509 条、避开 `0x7FFDC` 控制字；改动主机回归与三配置/clang-format/Host+ARM clang-tidy 通过，新 Debug App `0ff3b7a1…` 已部署并验证，Boot 未变。SSH 公钥接入 `192.168.10.12` 后，正常 D7 完成 509-record 满扇区回收并写入序号 1/offset 0。覆盖保存前后的 83.25 s Linux 抓取有 329 帧长度/CRC 正确、329 次 transfer 成功、0 overrun；原全 FF 参数与 metadata 尾部已恢复，核心 Running、无 fault。擦除重叠随后已由 MCU 侧 NVIC pending 探针验收，见 [B6记录](x1_b6_hil.md)。用户已确认外部看门狗超时为 60 s，取消 PA6 波形测量验收，该项不再阻塞 B6；既有每 3000 ms 输出 1 ms DONE 脉冲的策略保持。详见 [X1实施计划](x1_app_implementation_plan.md)、[产品契约](x1_product_contract.md) 和 [App构建说明](../App/README.md)。Boot链接容量固定 **32 KiB**，App起始地址仍为`0x00008000`；分节点提交前干净重编译的Boot Debug BIN与旧HIL基线不同，当前哈希见App构建说明，旧HIL不能替代当前镜像的硬件验收。用户已完成此前Boot和升级功能验证；结构调整的软件回归见 [project_structure.md](project_structure.md)。

下方为此前阶段的状态与验证记录，源码路径对应当时目录；原 `Core/Inc/boot_config.h` 现为 `Boot/Inc/boot_config.h`，原 `Protocol/` 现为 `Common/Protocol/`。

Baseline: node 1 safety baseline and Phase 2 inbound transport boundary closed with target evidence on 2026-08-25. This document describes reachable code, not only the feature macros in `Core/Inc/boot_config.h`.

## Implemented and reachable

- HC32F460xE startup, clock, millisecond timebase, reset-cause capture, power hold, external TPL5010 service, status LED and RTT/EasyLogger logging.
- Application-vector validation and immediate application handover after a normal reset when the vector is valid.
- Software-reset update window and invalid-application Recovery mode.
- Board-configured Boot/Linux I2C1 slave transport on PA3/SCL and PA2/SDA at address `0x11`, 400 kHz, with interrupt-driven RX/TX buffers. PA8/PA9 I2C2 belongs to the Application and is not initialized by Boot.
- Legacy `AA 44 18` framing, sequence complement, payload bounds and CRC16.
- Transport-independent protocol parsing through byte and chunk input APIs; `Protocol/` no longer polls or includes the I2C BSP.
- Main-loop RX orchestration through a fixed 544-byte transport buffer and `bsp_i2c_slave_read()`, which owns the RX ring critical section.
- HANDSHAKE, gated Application-region erase/download, per-chunk Flash readback comparison and gated jump after the ACK read completes.
- I2C address/byte/STOP/NACK/overflow/empty-read/busy/recovery counters and parser-timeout counters, with fault-triggered main-loop logging.
- I2C recovery only after an active transaction or hardware BUSY has made no progress for 2000 ms.
- HC32 ICG startup words are linked at `0x00000400`; the linker rejects a missing or incorrectly sized eight-word ICG block.
- CMake Debug, Release, ReleaseNoLog and HostTests presets.

## Feature-gate baseline

Default firmware builds enable real Flash erase/download and the upgrade jump command, with simulation disabled. Product-board end-to-end upgrade validation is still pending.

`ERASE` and `DOWNLOAD` dispatch require both their Legacy command macro and `BOOT_ENABLE_FLASH_UPDATE`. `JUMP` requires both `BOOT_ENABLE_LEGACY_JUMP` and `BOOT_ENABLE_APPLICATION_JUMP`. Simulation exercises enabled command state transitions without calling the Flash erase/write/read BSP.

## Current boot decision

| Application vector | Software reset | Mode |
| --- | --- | --- |
| invalid | either | Recovery |
| valid | yes | ten-minute update window |
| valid | no | immediate application handover |

The ten-minute inactivity timeout applies in both update-window and Recovery modes. Successful erase/download/accepted jump commands refresh the deadline. Expiry stops command processing and starts a nonblocking one-second fast-blink warning, then turns the LED off and deasserts PB3 power hold once. LED and watchdog polling continue if external power remains. See [status LED contract](status_led_contract.md) for error and handover indications.

## Current safety and completeness limits

- Image validity is only the initial MSP and reset-vector check. There is no complete-image length, CRC/hash, signature, version, anti-rollback or persistent VALID/UPDATING metadata.
- A valid-looking vector written after erase is enough for `JUMP_TO_APP`; contiguous/full-image coverage is not required.
- Immediate repeated-start reads remain unsupported; the host must end the write with STOP, wait for processing, then issue a separate read.
- Stalled-I2C recovery and application handover ordering still require target/HIL evidence. The normal final-byte NACK+STOP read-completion path is verified.
- Metadata A/B sectors are reserved in the map but unused.
- No current hardware/HIL run proves Flash update, power-loss behavior, I2C retry behavior, GPIO waveforms or application handover.
- Outbound response reservation/publication remains I2C-specific Phase 3 scope.

## Automated verification

Host CTest covers CRC, parser rejection/recovery/timeout, maximum frames, null and zero-length chunk input, frames split across chunks, multiple frames per chunk, Legacy golden bytes, enabled/disabled/simulation gates, TX busy ownership, ACK-before-jump, mocked Flash failures, memory ranges, vector validation and boot-mode selection. GitHub Actions builds host tests plus Release and ReleaseNoLog firmware. Host mocks do not prove target Flash, interrupts, timing or electrical behavior.

## Phase 2 evidence

- `Protocol/` accepts ordered byte chunks and has no BSP/I2C, raw RX-buffer or slave-state dependency.
- `main.c` owns BSP read -> parser chunk feed -> partial-frame timeout -> I2C recovery ordering.
- Debug, Release and ReleaseNoLog clean builds pass; Debug BSS is 12,984 bytes after replacing the parser-owned 1024-byte buffer with the 544-byte main-loop transport buffer.
- Debug BIN SHA-256: `819262842eb0a550be33687cddd2822e41c5fe950846a3067861b811b3c16ec4`; Debug ELF SHA-256: `d5d6af9bd11eb5c13682546bf6e8a057c9d4809d2f8203794b51a177fc0a93fc`.
- Debug, Release and ReleaseNoLog retain `.icg_sec` at `0x00000400`, size `0x20`, with the validated eight words. Host CTest passes 4/4.
- After a fresh reset, the exact I2C1 address `0x11`, 400 kHz HANDSHAKE returned `AA 44 18 01 FE 0C 00 20 00 00 00 00 00 00 00 00 00 00 00 A0 6E`.
- Post-read capture recorded RX/TX address matches `1/1`, RX/TX bytes `21/21`, one completed response read, released TX ownership, `PRIMASK=0`, no exception and no overflow, empty-read, busy, arbitration or recovery event.
- A preserved first attempt ran about 35 minutes 56 seconds after reset, beyond the ten-minute window, and returned 21 controlled `0xFF` bytes while the complete request remained queued. The passing retry was performed after reset within the documented window; the failure was not converted into a pass.

## Node 1 target evidence

- Final logging-enabled Debug image SHA-256: `0187072b1a1e4c582dda6a69b010179c8059b2b82ba5dad305040935de14dbde`.
- HC32F460xE pin-revised test board, I2C1 PA3/SCL and PA2/SDA, address `0x11`, 400 kHz: the 21-byte HANDSHAKE response matched exactly.
- Post-read capture recorded one RX and one TX address match, 21 RX bytes, 21 TX bytes, one completed response read, released TX ownership, `PRIMASK=0`, no active exception and no overflow, empty-read, busy, arbitration or recovery events.
- Release and ReleaseNoLog place `.icg_sec` at `0x00000400`, size `0x20`, with words `FFDFFFBF FFFFFEFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF`.
- Host CTest passed 4/4. Raw lab transcripts and safety preflights remain local under ignored `debug_artifacts/`; reusable J-Link commands live in `Tools/jlink/`.
