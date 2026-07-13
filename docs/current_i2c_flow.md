# Current I2C flow

No board I2C implementation exists in this checkout. The device header confirms I2C1 and event/interrupt sources RXI=420, TXI=421, TEI=422, and EEI=423. The DDL supports 7-bit slave addresses, RX-full, TX-empty, STOP, NACK, arbitration, and timeout status/interrupt controls.

The following are `UNKNOWN`: SDA/SCL pins and functions, NVIC channel assignments, bus timing, RX/TX buffer sizes, ISR/main sharing, STOP and NACK policy, overflow behavior, whether frames span writes, and whether Linux uses STOP/delay/read or repeated-start read. The baseline transport must keep pin-dependent hardware activation isolated until confirmed values are supplied.

