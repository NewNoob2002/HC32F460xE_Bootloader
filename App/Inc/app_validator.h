#ifndef APP_VALIDATOR_H
#define APP_VALIDATOR_H
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t (*app_vector_read_t)(uint32_t address, void* context);
bool app_validator_check(uint32_t app_base, app_vector_read_t read_word, void* context);
bool boot_application_vector_is_valid(void);
#endif
