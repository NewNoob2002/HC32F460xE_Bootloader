# Status-LED contract

PB5 is confirmed as the function LED, but its electrical polarity and output topology are absent from `LegacyReference/`. Hardware initialization therefore returns false and does not drive PB5.

The host-tested non-blocking logical patterns are: Booting on, update toggle every 250 ms, recovery toggle every 1000 ms, fatal toggle every 100 ms, and off before application jump. The scheduler supports active-high and active-low GPIO adapters and resets deterministically on mode changes. Once polarity is confirmed, the board adapter must preload the electrical off level before enabling CMOS or NMOS output as appropriate.

