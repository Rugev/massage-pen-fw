/* Persist user-requested vibration and heat levels for loading on startup. */
#ifndef USER_STORAGE_H
#define USER_STORAGE_H

/* Match the USER_SETTINGS region in STM32G030xx_FLASH.ld. */
#define STORAGE_USER_SETTINGS_BASE_ADDRESS      0x0800F800UL
#define STORAGE_USER_SETTINGS_SIZE_BYTES        2048U

/* Skeleton: storage format, public API and save/load behavior remain undecided. */

#endif /* USER_STORAGE_H */
