# External-watchdog contract

- Pin: PA6.
- Scheduled interval: 3000 ms.
- Device identity: UNKNOWN.
- Idle level: UNKNOWN.
- Active level: UNKNOWN.
- Pulse width: UNKNOWN.
- First-feed constraint: UNKNOWN.

`LegacyReference/` contains only the pin and interval. It does not establish a TPL5010-style DONE waveform or any other polarity/pulse contract. Consequently production PA6 output configuration and feeding are gated off and initialization returns false. The generic scheduler is non-blocking, wrap-safe, deadline-based, supports force-feed, and is host-tested with injected polarity and pulse width; it must not be connected to PA6 until the missing electrical values are confirmed.

Later Flash erase/program loops must call `bsp_external_watchdog_force_feed()` only after the board contract is enabled. At application handover, Boot leaves PA6 unchanged and transfers ownership to the application.

