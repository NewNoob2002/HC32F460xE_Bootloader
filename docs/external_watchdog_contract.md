# External-watchdog contract

Current implementation status: [current_status.md](current_status.md).

The external watchdog is a Texas Instruments TPL5010DDCT. PA6 drives its DONE input; RSTn drives MCU_RESET and WAKE is not connected to the MCU. The schematic setting is approximately 60 seconds.

A valid DONE event is a low-to-high transition. Boot holds PA6 low, emits a 1 ms high pulse every 3000 ms, then returns it low. The first deadline is initialization time plus 3000 ms. The 1 ms pulse exceeds the TPL5010 100 ns minimum.

The scheduler is non-blocking and wrap-safe. Late polling emits only one pulse and advances the intended deadline into the future, avoiding catch-up bursts. Force-feed succeeds only from idle and creates a new low-to-high transition.

`bsp_external_watchdog_prepare_handover()` can force PA6 low, but the current application-handover path does not call it. The application must reinitialize and service DONE promptly; the inherited PA6 level at entry is not currently guaranteed by the handover function.
