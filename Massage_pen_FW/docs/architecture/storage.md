# User settings storage

Store the user's requested vibration and heat levels in flash for loading on
startup. Use the `USER_SETTINGS` region in
[the linker script](../../STM32G030xx_FLASH.ld); its base address and size are
defined in [storage.h](../../User/Inc/storage.h).

[storage.c](../../User/Src/storage.c) is a skeleton. Storage format, save timing,
validation/defaults and startup integration remain undecided.
