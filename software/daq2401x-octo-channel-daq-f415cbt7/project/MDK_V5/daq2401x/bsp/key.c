#include "key.h"

// 按键列表与事件FIFO 
static key_handle_t key_list[KEY_MAX_NUM];
static uint8_t key_count = 0;

static key_event_item_t key_fifo[KEY_EVENT_FIFO_LENGTH];
static uint8_t fifo_head = 0;
static uint8_t fifo_tail = 0;
static uint8_t fifo_count = 0;

// 平台 GPIO 读取宏，低电平返回true
#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
#define KEY_READ(port, pin)   (HAL_GPIO_ReadPin((GPIO_TypeDef*)(port), (pin)) == GPIO_PIN_RESET)

#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#define KEY_READ(port, pin)   (gpio_input_data_bit_read((gpio_type*)(port), (pin)) == RESET)
#endif

/**
 * @brief  将一个按键事件压入事件FIFO
 *
 * 当FIFO未满时，将指定的按键ID和事件类型写入队列，并更新队列指针。
 * 若FIFO已满，则丢弃新事件并返回false。
 *
 * @param key_id 按键编号
 * @param evt    按键事件类型
 * @return true  压入成功
 * @return false FIFO已满，事件被丢弃
 */
static bool key_push_event(uint8_t key_id, key_event_t evt)
{
    if (fifo_count >= KEY_EVENT_FIFO_LENGTH)
    {
        return false;
    }
    else
    {
        fifo_count++; // 最快速度占用FIFO
        key_fifo[fifo_head].key_id = key_id;
        key_fifo[fifo_head].event  = evt;
        fifo_head = (fifo_head + 1) % KEY_EVENT_FIFO_LENGTH;
        return true;
    }
}

/**
 * @brief  从事件FIFO中获取一个按键事件
 *
 * 若FIFO中存在事件，则返回按键编号和事件类型，并释放一个队列空间。
 * 若FIFO为空，则返回false，key_id和event保持不变。
 *
 * @param key_id 指针，用于返回按键编号
 * @param event  指针，用于返回事件类型
 * @return true  成功获取事件
 * @return false FIFO为空，无事件可读
 */
bool key_get_event(uint8_t *key_id, key_event_t *event)
{
    if (fifo_count == 0)
        return false; // FIFO空

    *key_id = key_fifo[fifo_tail].key_id;
    *event  = key_fifo[fifo_tail].event;
    fifo_count--; // 最后释放FIFO

    fifo_tail = (fifo_tail + 1) % KEY_EVENT_FIFO_LENGTH;
    return true;
}

#if (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
/**
 * @brief  注册一个按键到按键管理数组
 *
 * 将指定的GPIO端口和引脚注册为一个按键，并分配用户定义的按键编号。
 * 按键数量受 KEY_MAX_NUM 限制，超过该数量时不会再注册新的按键。
 *
 * @param port   按键所在的GPIO端口
 * @param pin    按键对应的GPIO引脚号
 * @param key_id 用户定义的按键编号（禁止重复）
 */
void key_add(GPIO_TypeDef *port, uint16_t pin, uint8_t key_id)
{
    if (key_count < KEY_MAX_NUM)
    {
        key_list[key_count].port = port;
        key_list[key_count].pin = pin;
        key_list[key_count].counter = 0;
        key_list[key_count].key_id = key_id;
        key_count++;
    }
}
#elif (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
/**
 * @brief  注册一个按键到按键管理数组
 *
 * 将指定的GPIO端口和引脚注册为一个按键，并分配用户定义的按键编号。
 * 按键数量受 KEY_MAX_NUM 限制，超过该数量时不会再注册新的按键。
 *
 * @param port   按键所在的GPIO端口
 * @param pin    按键对应的GPIO引脚号
 * @param key_id 用户定义的按键编号（禁止重复）
 */
void key_add(gpio_type *port, uint16_t pin, uint8_t key_id)
{
    if (key_count < KEY_MAX_NUM)
    {
        key_list[key_count].port = port;
        key_list[key_count].pin = pin;
        key_list[key_count].counter = 0;
        key_list[key_count].key_id = key_id;
        key_count++;
    }
}
#endif

/**
 * @brief  扫描所有已注册的按键状态
 *
 * 每次调用时遍历所有按键，更新其计数器并判定事件：
 * - 按下时计数器递增，用于消抖和长短按判定；
 * - 松开时计数器清零；
 * - 当计数器达到短按阈值时触发一次短按事件；
 * - 当计数器达到长按阈值时触发一次长按事件，并将计数器归位到 KEY_LONG_REPEAT；
 * - 持续按下时，计数器超过长按阈值后会周期性触发长按事件。
 *
 * 建议在定时器中断或主循环中以固定周期调用该函数（例如20ms）。
 *
 * @return true  本次扫描中有事件产生
 * @return false 本次扫描中无事件产生
 */
bool key_scan(void)
{
    bool event_generated = false;

    for (uint8_t i = 0; i < key_count; i++)
    {
        key_handle_t *key = &key_list[i];
        key_event_t evt = key_event_none;

        uint8_t raw = KEY_READ(key->port, key->pin);

        if (raw)
        {
            if (key->counter < 255)
                key->counter++;

            if (key->counter == KEY_SHORT_THRESHOLD)
            {
                evt = key_event_short;
            }
            else if (key->counter == KEY_LONG_THRESHOLD)
            {
                evt = key_event_long;
                key->counter = KEY_LONG_REPEAT;
            }
        }
        else
        {
            key->counter = 0;
        }

        if (evt != key_event_none)
        {
            key_push_event(key->key_id, evt);
            event_generated = true;
        }
    }

    return event_generated;
}
