# External-watchdog contract

The external watchdog is a Texas Instruments TPL5010DDCT. PA6 drives its DONE input; RSTn drives MCU_RESET and WAKE is not connected to the MCU. The schematic setting is approximately 60 seconds.

A valid DONE event is a low-to-high transition. Boot holds PA6 low, emits a 1 ms high pulse every 3000 ms, then returns it low. The first deadline is initialization time plus 3000 ms. The 1 ms pulse exceeds the TPL5010 100 ns minimum.

The scheduler is non-blocking and wrap-safe. Late polling emits only one pulse and advances the intended deadline into the future, avoiding catch-up bursts. Force-feed succeeds only from idle and creates a new low-to-high transition. Application preparation immediately forces PA6 low without changing it to input/analog; the application then owns DONE servicing and must feed before the remaining hardware deadline.

