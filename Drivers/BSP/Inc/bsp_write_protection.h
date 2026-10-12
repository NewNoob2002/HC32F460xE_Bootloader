#ifndef BSP_WRITE_PROTECTION_H
#define BSP_WRITE_PROTECTION_H
#ifdef __cplusplus
extern "C" {
#endif
void bsp_write_protection_unlock(void);
void bsp_write_protection_restore(void);
#ifdef __cplusplus
}
#endif
#endif
