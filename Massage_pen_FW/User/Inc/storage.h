/* Persist user-requested vibration and heat levels for loading on startup. */
#ifndef USER_STORAGE_H
#define USER_STORAGE_H

/* Match the USER_SETTINGS region in STM32G030xx_FLASH.ld. */
#define STORAGE_USER_SETTINGS_BASE_ADDRESS      0x0800F800UL
#define STORAGE_USER_SETTINGS_SIZE_BYTES        2048U

#include <stdint.h>
#define STORAGE_DUMMY_LEVEL 1U
typedef struct { uint8_t heat_level, vibration_level; } Storage_Settings;
Storage_Settings Storage_Load(void);

#endif /* USER_STORAGE_H */
