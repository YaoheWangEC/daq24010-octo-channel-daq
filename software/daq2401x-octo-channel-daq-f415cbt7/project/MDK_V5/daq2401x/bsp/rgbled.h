/**
 * @brief RGB LED驱动库使用说明
 *
 * 本库用于控制RGB LED，支持共阴和共阳两种接法。
 *
 * version 1.1
 *
 * 使用步骤：
 * 1. 在工程中包含 `rgbled.h` 和 `rgbled.c`，注意需要选择合适的平台。
 * 2. 在 `rgbled.h` 中通过宏定义选择LED接法：
 *    - `#define RGBLED_COMMON_CATHODE` 共阴极（低电平点亮）
 *    - `#define RGBLED_COMMON_ANODE`   共阳极（高电平点亮）
 * 3. 调用 `RGBLED_Set(red, green, blue)` 控制LED状态。
 *
 * 注意事项：
 * - 参数支持 `LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD`。
 * - 确保GPIO已正确初始化并映射到红、绿、蓝三色引脚。
 */

#ifndef RGBLED_H
#define RGBLED_H

#include "platform.h"
#include "stdint.h"

#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#endif

// ============================= 模式选择 =============================
// 选择共阴或共阳模式（只能定义一个）
#define RGBLED_COMMON_CATHODE   // 共阴极：低电平点亮
// #define RGBLED_COMMON_ANODE   // 共阳极：高电平点亮

// ============================= LED状态宏 =============================
#define LED_OFF     0
#define LED_ON      1
#define LED_TOGGLE  2
#define LED_HOLD    3

// ============================= 函数声明 =============================
void rgbled_set(uint8_t red, uint8_t green, uint8_t blue);
void rgbled_test(void);

#endif // RGBLED_H
