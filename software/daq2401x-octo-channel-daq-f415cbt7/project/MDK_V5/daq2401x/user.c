#include "user.h"
#include "global_vars.h"
#include "bsp.h"
#include "command_io.h"
#include "command_fifo.h"
#include "command_parser.h"
#include "key.h"
#include "lcd.h"
#include "colorlut.h"
#include "wk_system.h"
#include "at32f415_wdt.h"

#include <stdio.h>
#include <string.h>

#define LINE0 3
#define LINE1 15
#define LINE2 27
#define LINE3 39
#define LINE4 51
#define LINE5 63

#define PLT_BACKGROUND 0
#define PLT_LINE 1
#define PLT_TEXT 2
#define PLT_RED 3
#define PLT_GREEN 4
#define PLT_BLUE 5
#define PLT_WAVE 6
#define PLT_GRID 7

static char string_buf[32];
static uint16_t palette[8] = {0x0000, 0x4208, 0xc618, 0xc000, 0x0600, 0x0018, 0xc600, 0x2945};

static bool wave_display = false;

static uint8_t frame_buf[160 * 80 / 2];   // 索引缓冲（4bit，160×80/2）；异步刷屏期间不得修改

static void key_handler(void);
static void display_handler(void);
static void draw_wave(const uint8_t *frame_buf);
static uint8_t wave_code_to_y(uint16_t code);

void setup(void)
{
    global_vars_init();

    // LCD 初始化
    lcd_init(); 
    lcd_fill(0, 0, 160, 80, 0x0000);
    lcd_show_string(0, LINE0, (const uint8_t *)"DAQ24010", 0x07ff, 0x0000, FONTSIZE_1206, 0);
    lcd_show_string(0, LINE1, (const uint8_t *)"octo-channel-universal-daq", 0xffe0, 0x0000, FONTSIZE_1206, 0);
    snprintf(string_buf, sizeof(string_buf),
             "%08X%08X%08X",
             (unsigned)g_vars.uid[0],
             (unsigned)g_vars.uid[1],
             (unsigned)g_vars.uid[2]);
    lcd_show_string(0, LINE2, (const uint8_t *)string_buf, 0xffff, 0x0000, FONTSIZE_1206, 0);
    lcd_show_string(0, LINE3, (const uint8_t *)"Hello World", 0xf81f, 0x0000, FONTSIZE_1206, 0);
    wk_delay_ms(1000);
    colorlut_init(palette, 8);

    command_fifo_init();
    command_io_init(&command_io_uart);
    command_parser_init();

    // 上电进行一次按键触发型采样
    adc_trigger_set(ADC_TRIGGER_MODE_SOFTWARE, 0);
    wave_display = true;
    adc_start();
}

void loop(void)
{
    static uint32_t last_refresh = 0;
    static bool flush_busy = false;   // 异步刷屏进行中标志

    wdt_counter_reload();   // 主循环喂狗

    command_parser_task();

    key_handler();

    // 异步刷屏驱动
    if (flush_busy == true)
    {
        // 刷屏进行中：仅推进状态机
        if (colorlut_flush_to_lcd_async(frame_buf, 0, 0, LCD_W, LCD_H) == 0)
        {
            flush_busy = false;   // 上一帧刷完，进入空闲
        }
    }
    else if (g_vars.tick - last_refresh >= 100)
    {
        // 空闲且到刷新节拍（10fps）：重绘一帧并启动刷屏
        last_refresh = g_vars.tick;
        display_handler();
        colorlut_flush_to_lcd_async(frame_buf, 0, 0, LCD_W, LCD_H);
        flush_busy = true;
    }
}

static void key_handler(void)
{
    uint8_t key_id;
    key_event_t evt;

    if (!key_get_event(&key_id, &evt))
    {
        return;
    }

    if (evt == key_event_short)
    {
        // 短按：ADC 空闲时强制软件触发，采集一次用于本地波形显示
        if (g_vars.acq_state == ADC_STATE_IDLE)
        {
            adc_trigger_set(ADC_TRIGGER_MODE_SOFTWARE, 0);
            wave_display = true;
            adc_start();
        }
    }
    else if (evt == key_event_long)
    {
        // 长按：切换下一通道。adc_start 内部会强制停止当前采集（若正在运行）
        uint8_t next = adc_logic_channel_get() % 8 + 1; // 1~8 循环
        adc_logic_channel_set_by_num(next);
        adc_trigger_set(ADC_TRIGGER_MODE_SOFTWARE, 0);
        wave_display = true;
        adc_start();
    }
}

static void display_handler(void)
{
    // 绘制背景
    colorlut_clear_buffer(frame_buf, LCD_W, LCD_H, PLT_BACKGROUND); // 清空全屏为背景色

    // 先画网格线（暗色），后画边框/0V 线（亮色）覆盖网格端点
    // 垂直网格线：水平时间方向 4 格（每格 2000 采样点），贯穿波形区
    colorlut_draw_line(frame_buf, 40, 0, 40, LCD_H - 1, PLT_GRID);
    colorlut_draw_line(frame_buf, 80, 0, 80, LCD_H - 1, PLT_GRID);
    colorlut_draw_line(frame_buf, 120, 0, 120, LCD_H - 1, PLT_GRID);

    // 水平网格线：电压方向 1V/div（8px/格），暗色网格；0V 线（y=40）后续单独画
    for (uint8_t y = 0; y <= 72; y += 8)
    {
        if (y == 40)
        {
            continue;
        }
        colorlut_draw_line(frame_buf, 0, y, LCD_W - 1, y, PLT_GRID);
    }

    // 屏幕外边框：最后画，覆盖网格线端点，保持边框完整
    colorlut_draw_rect(frame_buf, 0, 0, LCD_W, LCD_H, PLT_LINE);
    // 中间水平线：0 电压参考线
    colorlut_draw_line(frame_buf, 0, LCD_H / 2, LCD_W - 1, LCD_H / 2, PLT_LINE);

    // 仅 ADC 空闲时绘制波形
    if (g_vars.acq_state == ADC_STATE_IDLE)
    {
        draw_wave(frame_buf);
    }

    // 显示信息
    const char *state_str;
    uint8_t state_color;

    // 状态颜色与 RGBLED 一致：空闲=绿 / 等待触发=蓝 / 采集中=红
    switch (g_vars.acq_state)
    {
    case ADC_STATE_IDLE:
        state_str = "idle";
        state_color = PLT_GREEN;
        break;
    case ADC_STATE_WAIT_TRIGGER:
        state_str = "wait";
        state_color = PLT_BLUE;
        break;
    case ADC_STATE_ACQUIRING:
    default:
        state_str = "acq";
        state_color = PLT_RED;
        break;
    }

    // 左侧：通道 + 采样率 + AVDD（CH1 100ksps 3.291V）
    uint32_t avdd_mv = (uint32_t)(g_vars.avdd * 1000.0f);
    snprintf(string_buf, sizeof(string_buf), "CH%d %dksps %u.%03uV",
             (int)adc_logic_channel_get(),
             (int)(g_vars.decimated_samplerate / 1000),
             (unsigned)(avdd_mv / 1000),
             (unsigned)(avdd_mv % 1000));
    colorlut_show_string(frame_buf, 1, LINE0, (const uint8_t *)string_buf, PLT_TEXT, FONTSIZE_1206);

    // 右侧：状态，靠右对齐（1206 每字符宽 6px），右移 1px 避开右边框
    colorlut_show_string(frame_buf,
                         LCD_W - (uint16_t)strlen(state_str) * 6 - 1,
                         LINE0, (const uint8_t *)state_str, state_color, FONTSIZE_1206);

    // 最后一行：触发模式 + 阈值码字
    const char *trig_str;
    switch (g_vars.trigger_mode)
    {
    case ADC_TRIGGER_MODE_SOFTWARE:
        trig_str = "software";
        break;
    case ADC_TRIGGER_MODE_LEVEL_RISING:
        trig_str = "level+";
        break;
    case ADC_TRIGGER_MODE_LEVEL_FALLING:
        trig_str = "level-";
        break;
    case ADC_TRIGGER_MODE_SLOPE_RISING:
        trig_str = "slope+";
        break;
    case ADC_TRIGGER_MODE_SLOPE_FALLING:
        trig_str = "slope-";
        break;
    default:
        trig_str = "error";
        break;
    }
    snprintf(string_buf, sizeof(string_buf), "%s @ %d",
             trig_str, (int)g_vars.trigger_threshold);
    colorlut_show_string(frame_buf, 1, LINE5, (const uint8_t *)string_buf, PLT_TEXT, FONTSIZE_1206);

    // 右下角：时间刻度标注（每格 2000 采样点），右对齐避开右边框
    colorlut_show_string(frame_buf, LCD_W - 10 * 6 - 1, LINE5,
                         (const uint8_t *)"2k pts/div", PLT_TEXT, FONTSIZE_1206);

    // 倒数第二行右侧：电压刻度标注（每格 1V），与 2k/div 对称
    colorlut_show_string(frame_buf, LCD_W - 6 * 6 - 1, LINE4,
                         (const uint8_t *)"1V/div", PLT_TEXT, FONTSIZE_1206);

    // 注意：不再在此阻塞刷屏，由 loop 异步驱动 colorlut_flush_to_lcd_async
}

static void draw_wave(const uint8_t *frame_buf)
{
    static uint8_t wave_y_min[160];  // 每列 min 的 y 坐标（上包络）
    static uint8_t wave_y_max[160];  // 每列 max 的 y 坐标（下包络）
    static bool wave_valid = false;  // 首次提取成功后才置 true，避免 static 0 值误画

    // 仅按键触发完成采集时消耗一次缓冲区记录波形信息
    if (wave_display == true)
    {
        // 若 ringbuf 有本次按键采集的数据：一次性 pop 前 8000 点，每 50 点取 min/max
        if (!ringbuf_is_empty(&g_vars.adc_ring))
        {
            uint16_t total = ringbuf_len(&g_vars.adc_ring);
            uint16_t n = (total > 8000) ? 8000 : total;   // 显示前 8000 点
            uint16_t col, k;
            uint16_t v;

            for (col = 0; col < 160; col++)
            {
                uint16_t cnt = (col * 50 + 50 <= n) ? 50 : (n > col * 50 ? n - col * 50 : 0);
                uint16_t min_code = 4095;
                uint16_t max_code = 0;

                for (k = 0; k < cnt; k++)
                {
                    if (!ringbuf_pop(&g_vars.adc_ring, &v))
                    {
                        break;
                    }
                    if (v < min_code) min_code = v;
                    if (v > max_code) max_code = v;
                }

                // 电压越高 y 越小：上包络取 max_code，下包络取 min_code
                wave_y_min[col] = wave_code_to_y(max_code);
                wave_y_max[col] = wave_code_to_y(min_code);
            }

            // 清空剩余数据（>8000 部分不显示，直接丢弃）
            while (!ringbuf_is_empty(&g_vars.adc_ring))
            {
                ringbuf_pop(&g_vars.adc_ring, &v);
            }

            wave_valid = true;   // 提取成功
            wave_display = false;   // 仅提取成功后清零，避免低采样率下提前误清
        }
    }
    else
    {
        // do nothing
    }

    // 从未提取过有效波形：跳过绘制，避免 static 0 值画一条顶部横线
    if (!wave_valid)
    {
        return;
    }

    // 绘制：逐列画垂直包络线（255 = 超出 ±5V 显示区，不绘制）
    for (uint16_t col = 0; col < 160; col++)
    {
        uint8_t y_min = wave_y_min[col];
        uint8_t y_max = wave_y_max[col];

        if ((y_min == 255) || (y_max == 255))
        {
            continue;   // 该列超出显示区，跳过
        }
        colorlut_draw_line((uint8_t *)frame_buf, col, y_min, col, y_max, PLT_WAVE);
    }
}

static uint8_t wave_code_to_y(uint16_t code)
{
    // 第一步：码字 → 电压（float）。满量程 = 2×AVDD，AVDD 由 TL431 实时反推
    float volt = (float)((int16_t)code - 2048) * (2.0f * g_vars.avdd) / 2048.0f;

    // 第二步：电压 → 像素 y（0~79 对应 +5V ~ -5V，0V 居中 y=40，每 V 8px）
    float yf = 40.0f - volt * 8.0f;

    // 削顶削底不显示：超出 ±5V 显示区返回特殊值 255
    if ((yf < 0.0f) || (yf > 79.0f))
    {
        return 255;
    }

    return (uint8_t)(yf + 0.5f);   // 四舍五入
}
