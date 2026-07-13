# Application handover contract

Before the validated jump, Boot drives PB5 low/off, forces PA6 DONE low without changing its output mode, and reasserts PB3 high. It then disables/clears SysTick and Boot interrupts, relocates VTOR to `0x00008000`, sets MSP, and branches to the Thumb reset handler.

The application inherits PB3 high/output, PA6 low/output, PB5 low/output, PSPCR `0x0003`, and the existing clock state. It must immediately assume TPL5010 service ownership and explicitly reinitialize inherited GPIOs if its contract differs.

