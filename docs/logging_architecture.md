# Logging architecture

EasyLogger 2.2.99 from `Libraries/easylogger` writes through the Boot-specific `elog_port.c` into the single bundled SEGGER RTT implementation in `Libraries/SEGGER`. The RTT header identifies revision 25842 and the C source revision 29668; no separate semantic version is provided.

RTT uses channel 0, a 256-byte up buffer, a 16-byte down buffer, and `SEGGER_RTT_MODE_NO_BLOCK_SKIP`. Logging is never called from an ISR and failure does not block Boot. EasyLogger is bare-metal with lock callbacks disabled, no process/thread fields, no color, and a 256-byte line buffer. Format is `time level tag message`; release compiles through INFO and debug through DEBUG. File, function, line, process, thread, and floating-point formatting are disabled by policy.

Map-attributed RAM is 440 bytes for RTT (168-byte control block plus 272-byte RTT BSS) and 515 bytes directly for EasyLogger/port (256-byte line buffer, 248-byte state, 11-byte time buffer). The complete logging-enabled image costs 6248 additional Flash bytes and 1544 additional initialized/static RAM bytes versus the same board build without logging; this includes formatter/newlib dependencies.
