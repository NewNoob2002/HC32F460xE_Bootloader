# Documentation index

当前目录、共用模块和三产品 App 接入以 [project_structure.md](project_structure.md) 为准。`current_status.md` 下方保留早期验证记录，历史报告不代表本次结构调整已完成硬件复测。

X1 的架构、阶段依赖与验收条件见 [X1 App 实施计划](x1_app_implementation_plan.md)，下一轮（P3实板验收与P5）安排见 [x1_p5_plan.md](x1_p5_plan.md)，已确认依据与缺口见 [X1 产品契约](x1_product_contract.md)。当前`x1-p3`复用MultiButton并接入按键/电源/低电量与复位ACK流程，三配置构建和14/14主机回归通过，新镜像尚未烧录。旧`x1-hil1`的V1.3/V1.5实板证据分别见 [X1测试基线HIL](x1_hil_test_version.md) 和 [V1.5 MP实板基础HIL](x1_v15_hil.md)；不将旧HIL视为新P3的硬件验收。

The authoritative implementation status is [current_status.md](current_status.md). The cross-platform restructuring plan is [portability_plan.md](portability_plan.md). When an older investigation report conflicts with current source, the current-status document and source code take precedence.

## Current contracts

- Board and memory: [board_configuration.md](board_configuration.md), [boot_v2_memory_map.md](boot_v2_memory_map.md)
- Runtime: [application_handover_contract.md](application_handover_contract.md), [startup_interrupt_order.md](startup_interrupt_order.md), [power_hold_contract.md](power_hold_contract.md), [external_watchdog_contract.md](external_watchdog_contract.md), [status_led_contract.md](status_led_contract.md)
- Update path: [i2c_slave_contract.md](i2c_slave_contract.md), [hc32_efm_contract.md](hc32_efm_contract.md), [legacy_linux_transaction_sequence.md](legacy_linux_transaction_sequence.md)
- Verification: [build_report.md](build_report.md), [hardware_smoke_test.md](hardware_smoke_test.md), [known_limitations.md](known_limitations.md)

## Historical evidence

The HardFault, J-Link and legacy-audit documents preserve evidence from earlier revisions. They are not current capability claims:

- [debug_build_report.md](debug_build_report.md), [hardfault_root_cause.md](hardfault_root_cause.md), [jlink_hardfault_debug.md](jlink_hardfault_debug.md), [recovery_i2c_debug.md](recovery_i2c_debug.md)
- [legacy_flash_contract.md](legacy_flash_contract.md), [legacy_ota_audit.md](legacy_ota_audit.md), [legacy_reference_port_status.md](legacy_reference_port_status.md), [legacy_upload_contract.md](legacy_upload_contract.md)

## CI

`.github/workflows/ci.yml` runs host tests and builds Debug, Release and ReleaseNoLog Boot/X1 firmware on pushes to `main` and pull requests. Hardware behavior remains outside CI.
