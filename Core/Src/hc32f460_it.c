#include "hc32f460.h"
void NMI_Handler(void) {
    while (1) {
        __NOP();
    }
}
void HardFault_Handler(void) {
    while (1) {
        __NOP();
    }
}
