# External-watchdog contract

Current implementation status: [current_status.md](current_status.md).

The external watchdog is a Texas Instruments TPL5010DDCT. PA6 drives its DONE input; RSTn drives MCU_RESET and WAKE is not connected to the MCU. The schematic setting is approximately 60 seconds.

A valid DONE event is a low-to-high transition. Boot holds PA6 low, emits a 1 ms high pulse every 3000 ms, then returns it low. The first deadline is initialization time plus 3000 ms. The 1 ms pulse exceeds the TPL5010 100 ns minimum.

The scheduler is non-blocking and wrap-safe. Late polling emits only one pulse and advances the intended deadline into the future, avoiding catch-up bursts. Force-feed succeeds only from idle and creates a new low-to-high transition.

`bsp_external_watchdog_prepare_handover()` is called by the current application-handover path and forces PA6 low before entry. The application must reinitialize and service DONE promptly; X1 now reuses the scheduler after preparing its own timebase. Actual handover still needs target evidence.

For X1 acceptance, the user confirmed the 60 s external reset timeout and waived PA6 waveform measurement. The measured single-sector erase took about 18.05 ms, providing ample margin for this sample. Firmware continues to emit a nominal 1 ms DONE pulse every 3000 ms. The waveform requirement is waived; no electrical pulse measurement is claimed.
