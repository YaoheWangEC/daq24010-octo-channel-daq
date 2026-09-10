/**
 * @file lcd.h
 * @brief LCD BSP 驱动库接口说明
 *
 * 本库用于控制 ST7735S 0.96inch 80x160 LCD，
 * 同时兼容鸿讯电子 ST7735 0.96inch 80x160 LCD。
 * 提供基础的图形绘制和文字显示接口，支持点、线、矩形、圆、
 * 字符、字符串以及图片显示。
 *
 * version 4.1
 *
 * 使用步骤：
 * 1. 调用 `lcd_init` 初始化 LCD。
 * 2. 使用绘图函数如 `lcd_fill`、`lcd_draw_point`、`lcd_draw_line`、
 *    `lcd_draw_rectangle` 和 `lcd_draw_circle` 来绘制图形。
 * 3. 使用 `lcd_show_char`、`lcd_show_string` 显示字符和字符串。
 * 4. 使用 `lcd_show_picture` 显示图片。
 * 5. 可结合 ColorLUT GUI 驱动库实现大区域的多色压缩存储与刷新。
 *
 * 注意事项：
 * - 通过 SPI 接口发送数据，需要在 lcd_io.h 正确配置 SPI 外设，使用软件控制的CS脚。
 * - 若定义了 `HARDWARE_RESET`，则通过 GPIO 控制 LCD 的 RESET 引脚，映射到 GPIO: LCD_RESET。
 * - 4-wire SPI 模式下，DC 脚映射到 GPIO: LCD_DC，CS 脚映射到 GPIO: LCD_CS。
 * - 根据实际需求设置宏定义 `USE_HORIZONTAL`（屏幕方向）和 `HARDWARE_RESET`。
 * - 显示字符和字符串时请注意选择合适的字号和颜色。
 *
 * 提供的主要接口函数：
 * - `lcd_init`：初始化 LCD。
 * - `lcd_fill`：在指定区域填充颜色。
 * - `lcd_draw_point`：绘制单个像素点。
 * - `lcd_draw_line`：绘制直线。
 * - `lcd_draw_rectangle`：绘制矩形。
 * - `lcd_draw_circle`：绘制圆形。
 * - `lcd_show_char`：显示单个字符。
 * - `lcd_show_string`：显示字符串。
 * - `lcd_show_picture`：显示图片。
 *
 * 通过这些接口，可以在嵌入式系统中实现基本的图形和文字显示功能。
 */


#ifndef _LCD_H_
#define _LCD_H_

#include "stdint.h"

#define USE_HORIZONTAL 3  // 设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏
#define HARDWARE_RESET // 该系统通过GPIO:LCD_RESET连接到了LCD的RESET引脚

#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W 80
#define LCD_H 160
#else
#define LCD_W 160
#define LCD_H 80
#endif

// 定义颜色 (RGB565格式)
#define COLOR_WHITE          0xFFFF
#define COLOR_BLACK          0x0000
#define COLOR_BLUE           0x001F
#define COLOR_BRED           0XF81F
#define COLOR_GRED           0XFFE0
#define COLOR_GBLUE          0X07FF
#define COLOR_RED            0xF800
#define COLOR_MAGENTA        0xF81F
#define COLOR_GREEN          0x07E0
#define COLOR_CYAN           0x7FFF
#define COLOR_YELLOW         0xFFE0
#define COLOR_BROWN          0XBC40
#define COLOR_BRRED          0XFC07
#define COLOR_GRAY           0X8430
#define COLOR_DARKBLUE       0X01CF
#define COLOR_LIGHTBLUE      0X7D7C
#define COLOR_GRAYBLUE       0X5458
#define COLOR_LIGHTGREEN     0X841F
#define COLOR_LGRAY          0XC618
#define COLOR_LGRAYBLUE      0XA651
#define COLOR_LBBLUE         0X2B12


// 字体大小
#define FONTSIZE_1206 12
#define FONTSIZE_1608 16
#define FONTSIZE_2412 24
#define FONTSIZE_3216 32


// 函数声明

/**
 * @brief LCD的初始化函数
 *
 * 该函数用于初始化LCD显示器，包括发送各种初始化命令和设置参数。
 * 若定义了HARDWARE_RESET，则硬件对LCD屏幕复位。
 */
void lcd_init(void);

/**
 * @brief 在指定区域填充颜色
 *
 * @param x_start 起始列坐标
 * @param y_start 起始行坐标
 * @param x_end 终止列坐标
 * @param y_end 终止行坐标
 * @param color 要填充的颜色
 */
void lcd_fill(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

/**
 * @brief 在指定位置画点
 *
 * @param x 画点的X坐标
 * @param y 画点的Y坐标
 * @param color 点的颜色
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 画线
 *
 * @param x_start 起始X坐标
 * @param y_start 起始Y坐标
 * @param x_end 终止X坐标
 * @param y_end 终止Y坐标
 * @param color 线的颜色
 */
void lcd_draw_line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

/**
 * @brief 画矩形
 *
 * @param x_start 起始X坐标
 * @param y_start 起始Y坐标
 * @param x_end 终止X坐标
 * @param y_end 终止Y坐标
 * @param color 矩形的颜色
 */
void lcd_draw_rectangle(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

/**
 * @brief 画圆
 *
 * @param x_center 圆心的X坐标
 * @param y_center 圆心的Y坐标
 * @param radius 圆的半径
 * @param color 圆的颜色
 */
void lcd_draw_circle(uint16_t x_center, uint16_t y_center, uint8_t radius, uint16_t color);

/**
 * @brief 显示单个字符
 *
 * @param x 显示字符的X坐标
 * @param y 显示字符的Y坐标
 * @param character 要显示的字符
 * @param foreground_color 字的颜色
 * @param background_color 字的背景色
 * @param font_size 字号
 * @param mode 模式：0非叠加模式，1叠加模式
 */
void lcd_show_char(uint16_t x, uint16_t y, uint8_t character, uint16_t foreground_color, uint16_t background_color, uint8_t font_size, uint8_t mode);

/**
 * @brief 显示字符串
 *
 * @param x 显示字符串的X坐标
 * @param y 显示字符串的Y坐标
 * @param str 要显示的字符串
 * @param foreground_color 字的颜色
 * @param background_color 字的背景色
 * @param font_size 字号
 * @param mode 模式：0非叠加模式，1叠加模式
 */
void lcd_show_string(uint16_t x, uint16_t y, const uint8_t *str, uint16_t foreground_color, uint16_t background_color, uint8_t font_size, uint8_t mode);

/**
 * @brief 显示图片
 *
 * @param x 起点X坐标
 * @param y 起点Y坐标
 * @param length 图片长度
 * @param width 图片宽度
 * @param pic 图片数组
 */
void lcd_show_picture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[]);

#endif

