#include "global_vars.h"
#include "bsp.h"

#include <string.h>

global_vars_t g_vars;

static uint16_t adc_data_buf[ADC_DATA_BUF_LEN + 1]; // 采集数据环形缓冲存储区

/**
 * @brief  全局变量初始化
 * @details 将 g_vars 全部归零，然后设置默认值。
 */
void global_vars_init(void)
{
    memset(&g_vars, 0x00, sizeof(g_vars));

    ringbuf_init(&g_vars.adc_ring, adc_data_buf, ADC_DATA_BUF_LEN);

    bsp_init();

    g_vars.uid[0] = *(volatile uint32_t *)0x1FFFF7E8;
    g_vars.uid[1] = *(volatile uint32_t *)0x1FFFF7EC;
    g_vars.uid[2] = *(volatile uint32_t *)0x1FFFF7F0;
    g_vars.tick = 0;

    g_vars.avdd = 3.3f;

}
