/**
 * @file colorlut.h
 * @brief ColorLUT 图形显示驱动库接口说明
 *
 * 本库提供基于调色板索引的图形显示接口，通过 DMA 批传输刷新到 LCD，
 * 适合在嵌入式系统中实现高效的 GUI。
 * 调色板大小默认 16 色（4bit 打包，已实测）；256 色模式可通过宏切换
 * （8bit 逐像素），当前未验证。
 *
 * 使用步骤：
 * 1. 调用 `colorlut_init` 初始化调色板颜色。
 * 2. 创建索引缓冲区，并使用 `colorlut_clear_buffer` 清屏。
 * 3. 使用绘图函数如 `colorlut_set_pixel`、`colorlut_draw_line`、
 *    `colorlut_draw_rect`、`colorlut_show_string` 绘制图形和文字。
 * 4. 调用 `colorlut_flush_to_lcd` 将缓冲区内容刷新到 LCD 显示。
 *
 * 注意事项：
 * - 通过宏 `COLORLUT_MAX_COLORS` 选择调色板大小 (16 或 256)；
 *   16 色（默认，4bit 打包）已实测，256 色分支尚未验证。
 * - 在 16 色模式下，每字节存储两个像素索引；在 256 色模式下，每字节存储一个像素索引。
 * - 刷新采用行扫描（逐行）方式，数据序与 LCD 地址递增方向一致（列扫描已移除）。
 * - 使用前需确保 LCD 已初始化，且 DMA 批传输可用。
 * - 本库仅提供基于索引的绘制接口，实际颜色由调色板映射到 RGB565。
 *
 * 提供的主要接口函数：
 * - `colorlut_init`：初始化调色板。
 * - `colorlut_get_color` / `colorlut_set_color`：获取或设置调色板颜色。
 * - `colorlut_clear_buffer`：清空索引缓冲区。
 * - `colorlut_set_pixel` / `colorlut_get_pixel`：设置或获取像素索引。
 * - `colorlut_draw_line` / `colorlut_draw_rect`：绘制直线和矩形。
 * - `colorlut_show_char` / `colorlut_show_string`：显示字符和字符串。
 * - `colorlut_flush_to_lcd`：将索引缓冲区刷新到 LCD。
 *
 * 通过这些接口，可以在嵌入式系统中实现轻量级的 GUI 绘制与显示。
 */

#ifndef COLORLUT_H
#define COLORLUT_H

#include <stdint.h>
#include <stddef.h>

#include "lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

// 最大调色板大小：16(4bit打包，已实测) 或 256(8bit逐像素，未验证)
#define COLORLUT_MAX_COLORS 16

// 区域缓冲宽度：既作为索引缓冲的行距（stride，set/get_pixel 用它寻址），
// 也决定 DMA 行缓冲大小；使用非全屏缓冲时须改为该缓冲的实际宽度。
#define COLORLUT_AREA_WIDTH LCD_W
// 区域缓冲长度（当前代码未用，仅作尺寸约定参考）
#define COLORLUT_AREA_HEIGHT LCD_H

/**
 * @brief 初始化调色板
 *
 * @param colors RGB565颜色数组
 * @param count  调色板颜色数量 (最大16或256；16 已实测，256 未验证)
 */
void colorlut_init(const uint16_t *colors, size_t count);

/**
 * @brief 获取调色板中的颜色
 *
 * @param index 调色板索引
 * @return uint16_t RGB565颜色
 */
uint16_t colorlut_get_color(uint8_t index);

/**
 * @brief 设置调色板中的某个颜色
 *
 * @param index 调色板索引
 * @param color RGB565颜色
 */
void colorlut_set_color(uint8_t index, uint16_t color);

/**
 * @brief 清空索引缓冲区
 *
 * @param buf    索引缓冲区指针
 * @param width  图像宽度
 * @param height 图像高度
 * @param color_index 填充颜色索引
 */
void colorlut_clear_buffer(uint8_t *buf, size_t width, size_t height, uint8_t color_index);

/**
 * @brief 设置某个像素的索引值
 *
 * @param buf 索引缓冲区指针
 * @param x   像素X坐标
 * @param y   像素Y坐标
 * @param color_index 调色板索引
 */
void colorlut_set_pixel(uint8_t *buf, size_t x, size_t y, uint8_t color_index);

/**
 * @brief 获取某个像素的索引值
 *
 * @param buf 索引缓冲区指针
 * @param x   像素X坐标
 * @param y   像素Y坐标
 * @return uint8_t 调色板索引
 */
uint8_t colorlut_get_pixel(const uint8_t *buf, size_t x, size_t y);

/**
 * @brief 绘制直线
 *
 * @param buf 索引缓冲区
 * @param x0  起点X
 * @param y0  起点Y
 * @param x1  终点X
 * @param y1  终点Y
 * @param color_index 调色板索引
 */
void colorlut_draw_line(uint8_t *buf, size_t x0, size_t y0, size_t x1, size_t y1, uint8_t color_index);

/**
 * @brief 绘制空心矩形
 *
 * @param buf 索引缓冲区
 * @param x   左上角X
 * @param y   左上角Y
 * @param w   宽度
 * @param h   高度
 * @param color_index 调色板索引
 */
void colorlut_draw_rect(uint8_t *buf, size_t x, size_t y, size_t w, size_t h, uint8_t color_index);

/**
 * @brief 在索引缓冲区中显示单个字符（叠加模式）
 *
 * @param buf 索引缓冲区指针
 * @param x 显示字符的屏幕X坐标
 * @param y 显示字符的屏幕Y坐标
 * @param character 要显示的字符
 * @param color_index 调色板索引（前景色）
 * @param font_size 字号
 */
void colorlut_show_char(uint8_t *buf, uint16_t x, uint16_t y, uint8_t character, uint8_t color_index, uint8_t font_size);

/**
 * @brief 在索引缓冲区中显示字符串（叠加模式）
 *
 * @param buf 索引缓冲区指针
 * @param x 显示字符串的屏幕X坐标
 * @param y 显示字符串的屏幕Y坐标
 * @param str 要显示的字符串
 * @param color_index 调色板索引（前景色）
 * @param font_size 字号
 */
void colorlut_show_string(uint8_t *buf, uint16_t x, uint16_t y, const uint8_t *str, uint8_t color_index, uint8_t font_size);

/**
 * @brief 将索引缓冲区内容刷新到LCD显示器（自包含缓冲，可非全屏）
 *
 * 缓冲区采用自身局部坐标（原点为缓冲左上角，行距 = COLORLUT_AREA_WIDTH），
 * x_start/y_start 仅表示该区域贴在屏幕上的目标位置，不参与缓冲寻址；
 * 因此缓冲可为任意局部区域，宽度无须等于屏宽。
 *
 * @param src     索引缓冲区指针（内容按缓冲局部坐标绘制，原点为缓冲左上角）
 * @param x_start 该区域在屏幕上的目标起始X坐标
 * @param y_start 该区域在屏幕上的目标起始Y坐标
 * @param width   该区域的宽度（像素数，<= COLORLUT_AREA_WIDTH）
 * @param height  该区域的高度（像素数）
 */
void colorlut_flush_to_lcd(const uint8_t *src, size_t x_start, size_t y_start, size_t width, size_t height);

/**
 * @brief 异步刷屏（无栈状态机 + pingpong 缓冲，非阻塞）
 *
 * 每次调用推进一次状态机：返回 0 表示空闲（本次入参作为新帧开始刷屏），
 * 返回非 0 表示刷屏进行中（本次入参被忽略，仅推进当前帧）。
 * 适用于主循环周期驱动，避免长时间阻塞。语义与阻塞版相同：自包含缓冲、
 * 局部坐标、x_start/y_start 为屏幕目标位。
 *
 * @param src     索引缓冲区指针（内容按缓冲局部坐标绘制，原点为缓冲左上角）
 * @param x_start 该区域在屏幕上的目标起始X坐标
 * @param y_start 该区域在屏幕上的目标起始Y坐标
 * @param width   该区域的宽度（像素数，<= COLORLUT_AREA_WIDTH）
 * @param height  该区域的高度（像素数）
 * @retval 0=空闲可接收新帧；非0=刷屏进行中
 */
int colorlut_flush_to_lcd_async(const uint8_t *src, size_t x_start, size_t y_start, size_t width, size_t height);


#ifdef __cplusplus
}
#endif

#endif // COLORLUT_H
