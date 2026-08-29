#ifndef PLATFORM_H
#define PLATFORM_H

// ============================= 设备类型 =============================
#define DEVICE_STM32   0x00
#define DEVICE_AT32    0x01

// ============================= 驱动类型 =============================
#define DRIVER_CMSIS   0x00
#define DRIVER_SPL     0x01
#define DRIVER_HAL     0x02
#define DRIVER_LL      0x03

// ============================= 平台选择 =============================
#define PLATFORM_DEVICE   DEVICE_AT32
#define PLATFORM_DRIVER   DRIVER_SPL

#endif
