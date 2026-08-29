#ifndef KEY_H
#define KEY_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"

#if(PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
#include "stm32g4xx_hal.h"
#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#include "at32f415_gpio.h"
#endif

#define KEY_MAX_NUM              5   // 最大按键数量
#define KEY_EVENT_FIFO_LENGTH    10  // 事件FIFO深度

#define KEY_SHORT_THRESHOLD      5   // 短按状态触发阈值
#define KEY_LONG_THRESHOLD       50  // 长按状态触发阈值
#define KEY_LONG_REPEAT          5   // 长按触发归位值

typedef enum 
{
    key_event_none = 0,
    key_event_short,
    key_event_long
} key_event_t;

typedef struct 
{
    uint8_t key_id;
    key_event_t event;
} key_event_item_t;

#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
typedef struct 
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t counter;
    uint8_t key_id;
} key_handle_t;

#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
typedef struct 
{
    gpio_type *port;
    uint16_t pin;
    uint8_t counter;
    uint8_t key_id;
} key_handle_t;
#endif

#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
void key_add(GPIO_TypeDef *port, uint16_t pin, uint8_t key_id); // 注册一个按键到按键管理数组
bool key_scan(void);                                            // 扫描所有已注册的按键状态
bool key_get_event(uint8_t *key_id, key_event_t *event);        // 从事件FIFO中获取一个按键事件

#elif(PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
void key_add(gpio_type *port, uint16_t pin, uint8_t key_id);    // 注册一个按键到按键管理数组
bool key_scan(void);                                            // 扫描所有已注册的按键状态
bool key_get_event(uint8_t *key_id, key_event_t *event);        // 从事件FIFO中获取一个按键事件
#endif

#endif
