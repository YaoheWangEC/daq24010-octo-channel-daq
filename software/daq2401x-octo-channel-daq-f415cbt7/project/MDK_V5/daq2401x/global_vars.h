#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include "ringbuf.h"

#include <stdint.h>
#include <stdbool.h>


#define ADC_DMA_BUF_LEN 320
#define ADC_DATA_BUF_LEN 8192
#define VREF 2.495f

// 片外 ADC 逻辑输入通道与 ADC 物理通道（MUX 地址）的映射
typedef enum
{
    ADC_LOGIC_CHANNEL_1 = 7,
    ADC_LOGIC_CHANNEL_2 = 6,
    ADC_LOGIC_CHANNEL_3 = 5,
    ADC_LOGIC_CHANNEL_4 = 0,
    ADC_LOGIC_CHANNEL_5 = 1,
    ADC_LOGIC_CHANNEL_6 = 2,
    ADC_LOGIC_CHANNEL_7 = 3,
    ADC_LOGIC_CHANNEL_8 = 4
} adc_logic_channel_t;

// 片外 ADC 的真实采样率
typedef enum
{
    ADC_RAW_SAMPLERATE_100KSPS = 100000,
    ADC_RAW_SAMPLERATE_120KSPS = 120000,
    ADC_RAW_SAMPLERATE_200KSPS = 200000
} adc_raw_samplerate_t;

// 片外 ADC 降采样后的目标输出采样率
typedef enum
{
    ADC_DECIMATED_SAMPLERATE_1KSPS   = 1000,
    ADC_DECIMATED_SAMPLERATE_2KSPS   = 2000,
    ADC_DECIMATED_SAMPLERATE_3KSPS   = 3000,
    ADC_DECIMATED_SAMPLERATE_4KSPS   = 4000,
    ADC_DECIMATED_SAMPLERATE_5KSPS   = 5000,
    ADC_DECIMATED_SAMPLERATE_6KSPS   = 6000,
    ADC_DECIMATED_SAMPLERATE_8KSPS   = 8000,
    ADC_DECIMATED_SAMPLERATE_10KSPS  = 10000,
    ADC_DECIMATED_SAMPLERATE_20KSPS  = 20000,
    ADC_DECIMATED_SAMPLERATE_30KSPS  = 30000,
    ADC_DECIMATED_SAMPLERATE_40KSPS  = 40000,
    ADC_DECIMATED_SAMPLERATE_50KSPS  = 50000,
    ADC_DECIMATED_SAMPLERATE_60KSPS  = 60000,
    ADC_DECIMATED_SAMPLERATE_100KSPS = 100000,
    ADC_DECIMATED_SAMPLERATE_120KSPS = 120000,
    ADC_DECIMATED_SAMPLERATE_200KSPS = 200000
} adc_decimated_samplerate_t;

// 片外 ADC 触发方式
typedef enum
{
    ADC_TRIGGER_MODE_SOFTWARE = 0,   /* 软件触发：开始采集即触发 */
    ADC_TRIGGER_MODE_LEVEL_RISING,   /* 电平触发·上升沿 */
    ADC_TRIGGER_MODE_LEVEL_FALLING,  /* 电平触发·下降沿 */
    ADC_TRIGGER_MODE_SLOPE_RISING,   /* 斜率触发·上升沿 */
    ADC_TRIGGER_MODE_SLOPE_FALLING   /* 斜率触发·下降沿 */
} adc_trigger_mode_t;

// 片外 ADC 采集状态
typedef enum
{
    ADC_STATE_IDLE = 0,          /* 空闲：缓冲区满 / ADC 转换结束 */
    ADC_STATE_WAIT_TRIGGER,      /* 等待触发 */
    ADC_STATE_ACQUIRING          /* 采集中 */
} adc_acq_state_t;

typedef struct 
{
    uint32_t uid[3];    /**< 器件 UID */
    uint32_t tick;      /**< 1ms tick */

    adc_logic_channel_t logic_channel;                  /**< 当前选择的片外 ADC 逻辑通道 */
    adc_raw_samplerate_t raw_samplerate;                /**< 片外 ADC 的真实采样率 */
    adc_decimated_samplerate_t decimated_samplerate;    /**< 降采样目标输出采样率 */
    adc_trigger_mode_t trigger_mode;                    /**< 触发方式（软件/电平/斜率×上升/下降沿） */
    int16_t trigger_threshold;                          /**< 触发阈值（电平或斜率，0~4095） */
    adc_acq_state_t acq_state;                          /**< 采集状态（空闲/等待触发/采集中） */
    float avdd;                                         /**< 当前片外 ADC 的满量程电压 */

    uint16_t adc_dma_tx_buf[ADC_DMA_BUF_LEN];           /**< 片外 ADC DMA TX 缓冲区 */
    uint16_t adc_dma_rx_buf[ADC_DMA_BUF_LEN];           /**< 片外 ADC DMA RX 缓冲区*/
    ringbuf_t adc_ring;                                 /**< 采集数据环形缓冲 */

} global_vars_t;

extern global_vars_t g_vars;

/**
 * @brief  全局变量初始化
 * @details 将 g_vars 全部归零，然后设置默认值。
 */
void global_vars_init(void);

#endif
