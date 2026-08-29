#include "rgbled.h"

#if (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#include "at32f415_wk_config.h"
#include "wk_system.h"
#elif (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
#include "main.h"
#include "gpio.h"
#endif

#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)

/**
 * @brief 设置RGB LED状态
 *
 * @param red   LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 * @param green LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 * @param blue  LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 */
void rgbled_set(uint8_t red, uint8_t green, uint8_t blue)
{
    // 红灯
#if defined(RGBLED_COMMON_CATHODE)
    if (red == LED_ON)
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    else if (red == LED_OFF)
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
#elif defined(RGBLED_COMMON_ANODE)
    if (red == LED_ON)
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    else if (red == LED_OFF)
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
#endif
    if (red == LED_TOGGLE)
        HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);

    // 绿灯
#if defined(RGBLED_COMMON_CATHODE)
    if (green == LED_ON)
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    else if (green == LED_OFF)
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
#elif defined(RGBLED_COMMON_ANODE)
    if (green == LED_ON)
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    else if (green == LED_OFF)
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
#endif
    if (green == LED_TOGGLE)
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    // 蓝灯
#if defined(RGBLED_COMMON_CATHODE)
    if (blue == LED_ON)
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
    else if (blue == LED_OFF)
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
#elif defined(RGBLED_COMMON_ANODE)
    if (blue == LED_ON)
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    else if (blue == LED_OFF)
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
#endif
    if (blue == LED_TOGGLE)
        HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
}

/**
 * @brief RGB LED 测试函数（仅调试使用）
 *        通过遍历所有状态验证 rgbled_set() 的所有分支逻辑
 */
void rgbled_test(void)
{
    uint8_t state[6] = {LED_OFF, LED_ON, LED_TOGGLE, LED_TOGGLE, LED_TOGGLE, LED_TOGGLE};
    
    while (1)
    {
        rgbled_set(LED_OFF, LED_OFF, LED_OFF);

        // 红灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(state[i], LED_HOLD, LED_HOLD);
            HAL_Delay(200);
        }

        // 绿灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(LED_HOLD, state[i], LED_HOLD);
            HAL_Delay(200);
        }

        // 蓝灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(LED_HOLD, LED_HOLD, state[i]);
            HAL_Delay(200);
        }

        // RGB 全亮 / 全灭
        rgbled_set(LED_OFF, LED_OFF, LED_OFF);
        HAL_Delay(1000);
        rgbled_set(LED_ON, LED_ON, LED_ON);
        HAL_Delay(1000);
    }
}

#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)

/**
 * @brief 设置RGB LED状态
 *
 * @param red   LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 * @param green LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 * @param blue  LED_OFF / LED_ON / LED_TOGGLE / LED_HOLD
 */
void rgbled_set(uint8_t red, uint8_t green, uint8_t blue)
{
    // 红灯
#if defined(RGBLED_COMMON_CATHODE)
    if (red == LED_ON)
        gpio_bits_reset(LED_RED_GPIO_PORT, LED_RED_PIN);
    else if (red == LED_OFF)
        gpio_bits_set(LED_RED_GPIO_PORT, LED_RED_PIN);
#elif defined(RGBLED_COMMON_ANODE)
    if (red == LED_ON)
        gpio_bits_set(LED_RED_GPIO_PORT, LED_RED_PIN);
    else if (red == LED_OFF)
        gpio_bits_reset(LED_RED_GPIO_PORT, LED_RED_PIN);
#endif
    if (red == LED_TOGGLE)
        gpio_bits_toggle(LED_RED_GPIO_PORT, LED_RED_PIN);

    // 绿灯
#if defined(RGBLED_COMMON_CATHODE)
    if (green == LED_ON)
        gpio_bits_reset(LED_GREEN_GPIO_PORT, LED_GREEN_PIN);
    else if (green == LED_OFF)
        gpio_bits_set(LED_GREEN_GPIO_PORT, LED_GREEN_PIN);
#elif defined(RGBLED_COMMON_ANODE)
    if (green == LED_ON)
        gpio_bits_set(LED_GREEN_GPIO_PORT, LED_GREEN_PIN);
    else if (green == LED_OFF)
        gpio_bits_reset(LED_GREEN_GPIO_PORT, LED_GREEN_PIN);
#endif
    if (green == LED_TOGGLE)
        gpio_bits_toggle(LED_GREEN_GPIO_PORT, LED_GREEN_PIN);

    // 蓝灯
#if defined(RGBLED_COMMON_CATHODE)
    if (blue == LED_ON)
        gpio_bits_reset(LED_BLUE_GPIO_PORT, LED_BLUE_PIN);
    else if (blue == LED_OFF)
        gpio_bits_set(LED_BLUE_GPIO_PORT, LED_BLUE_PIN);
#elif defined(RGBLED_COMMON_ANODE)
    if (blue == LED_ON)
        gpio_bits_set(LED_BLUE_GPIO_PORT, LED_BLUE_PIN);
    else if (blue == LED_OFF)
        gpio_bits_reset(LED_BLUE_GPIO_PORT, LED_BLUE_PIN);
#endif
    if (blue == LED_TOGGLE)
        gpio_bits_toggle(LED_BLUE_GPIO_PORT, LED_BLUE_PIN);
}

/**
 * @brief RGB LED 测试函数（仅调试使用）
 *        通过遍历所有状态验证 rgbled_set() 的所有分支逻辑
 */
void rgbled_test(void)
{
    uint8_t state[6] = {LED_OFF, LED_ON, LED_TOGGLE, LED_TOGGLE, LED_TOGGLE, LED_TOGGLE};
    
    while (1)
    {
        rgbled_set(LED_OFF, LED_OFF, LED_OFF);

        // 红灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(state[i], LED_HOLD, LED_HOLD);
            wk_delay_ms(200);
        }

        // 绿灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(LED_HOLD, state[i], LED_HOLD);
            wk_delay_ms(200);
        }

        // 蓝灯测试
        for (int i = 0; i < 6; i++)
        {
            rgbled_set(LED_HOLD, LED_HOLD, state[i]);
            wk_delay_ms(200);
        }

        // RGB 全亮 / 全灭
        rgbled_set(LED_OFF, LED_OFF, LED_OFF);
        wk_delay_ms(1000);
        rgbled_set(LED_ON, LED_ON, LED_ON);
        wk_delay_ms(1000);
    }
}
#endif
