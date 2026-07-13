# Application handover contract

Before application entry, Boot turns the logical status LED off when available, performs only a non-blocking logging flush, reasserts PB3, disables and clears SysTick including pending state, disables/clears Boot NVIC channels, sets VTOR to `0x00008000`, loads MSP, and branches to the validated Thumb reset handler.

Inherited state is PB3 high as CMOS output; PA6 and PB5 untouched while their contracts are unconfirmed; PSPCR `0x03` with SWCLK/SWDIO selected; PH0/PH1 unchanged by the minimal clock observer. The application must explicitly accept or reinitialize these states and assume ownership of watchdog feeding. Application base remains `0x00008000`.

