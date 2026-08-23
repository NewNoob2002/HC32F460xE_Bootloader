# Documentation index

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

`.github/workflows/ci.yml` runs host tests and builds Release plus ReleaseNoLog firmware on pushes to `main` and pull requests. Hardware behavior remains outside CI.
