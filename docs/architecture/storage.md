# User settings storage

Store the user's requested vibration and heat levels in flash for loading on
startup. Use the `USER_SETTINGS` region in
[the linker script](../../Massage_pen_FW/STM32G030xx_FLASH.ld); its base address and size are
defined in [storage.h](../../Massage_pen_FW/User/Inc/storage.h).

[storage.c](../../Massage_pen_FW/User/Src/storage.c) is a skeleton. Storage format, save timing,
validation/defaults and startup integration remain undecided.
