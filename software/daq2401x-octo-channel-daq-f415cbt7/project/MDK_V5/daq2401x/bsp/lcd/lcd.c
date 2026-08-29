#include "lcd.h"
#include "lcdfont.h"
#include "lcd_io.h"

/**
 * @brief LCD的初始化函数
 *
 * 该函数用于初始化LCD显示器，包括发送各种初始化命令和设置参数。
 * 若定义了HARDWARE_RESET，则硬件对LCD屏幕复位。
 */
void lcd_init(void)
{
    lcdio_init();
#ifdef HARDWARE_RESET
    lcdio_reset(true);
    lcdio_delay_ms(100);
    lcdio_reset(false);
    lcdio_delay_ms(100);
#endif

    lcdio_delay_ms(100);

    lcdio_write_reg(0x11);     // Sleep out
    lcdio_delay_ms(120);       // Delay 120ms
    lcdio_write_reg(0xB1);     // Normal mode
    lcdio_write_byte(0x05);
    lcdio_write_byte(0x3C);
    lcdio_write_byte(0x3C);
    lcdio_write_reg(0xB2);     // Idle mode
    lcdio_write_byte(0x05);
    lcdio_write_byte(0x3C);
    lcdio_write_byte(0x3C);
    lcdio_write_reg(0xB3);     // Partial mode
    lcdio_write_byte(0x05);
    lcdio_write_byte(0x3C);
    lcdio_write_byte(0x3C);
    lcdio_write_byte(0x05);
    lcdio_write_byte(0x3C);
    lcdio_write_byte(0x3C);
    lcdio_write_reg(0xB4);     // Dot inversion
    lcdio_write_byte(0x03);
    lcdio_write_reg(0xC0);     // AVDD GVDD
    lcdio_write_byte(0xAB);
    lcdio_write_byte(0x0B);
    lcdio_write_byte(0x04);
    lcdio_write_reg(0xC1);     // VGH VGL
    lcdio_write_byte(0xC5);   // C0
    lcdio_write_reg(0xC2);     // Normal Mode
    lcdio_write_byte(0x0D);
    lcdio_write_byte(0x00);
    lcdio_write_reg(0xC3);     // Idle
    lcdio_write_byte(0x8D);
    lcdio_write_byte(0x6A);
    lcdio_write_reg(0xC4);     // Partial+Full
    lcdio_write_byte(0x8D);
    lcdio_write_byte(0xEE);
    lcdio_write_reg(0xC5);     // VCOM
    lcdio_write_byte(0x0F);
    lcdio_write_reg(0xE0);     // positive gamma
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x0E);
    lcdio_write_byte(0x08);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x10);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x02);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x09);
    lcdio_write_byte(0x0F);
    lcdio_write_byte(0x25);
    lcdio_write_byte(0x36);
    lcdio_write_byte(0x00);
    lcdio_write_byte(0x08);
    lcdio_write_byte(0x04);
    lcdio_write_byte(0x10);
    lcdio_write_reg(0xE1);     // negative gamma
    lcdio_write_byte(0x0A);
    lcdio_write_byte(0x0D);
    lcdio_write_byte(0x08);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x0F);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x02);
    lcdio_write_byte(0x07);
    lcdio_write_byte(0x09);
    lcdio_write_byte(0x0F);
    lcdio_write_byte(0x25);
    lcdio_write_byte(0x35);
    lcdio_write_byte(0x00);
    lcdio_write_byte(0x09);
    lcdio_write_byte(0x04);
    lcdio_write_byte(0x10);

    lcdio_write_reg(0xFC);
    lcdio_write_byte(0x80);

    lcdio_write_reg(0x3A);
    lcdio_write_byte(0x05);
    lcdio_write_reg(0x36);
    if (USE_HORIZONTAL == 0) lcdio_write_byte(0x08);
    else if (USE_HORIZONTAL == 1) lcdio_write_byte(0xC8);
    else if (USE_HORIZONTAL == 2) lcdio_write_byte(0x78);
    else lcdio_write_byte(0xA8);
    lcdio_write_reg(0x21);     // Display inversion
    lcdio_write_reg(0x29);     // Display on
    lcdio_write_reg(0x2A);     // Set Column Address
    lcdio_write_byte(0x00);
    lcdio_write_byte(0x1A);  // 26
    lcdio_write_byte(0x00);
    lcdio_write_byte(0x69);   // 105
    lcdio_write_reg(0x2B);     // Set Page Address
    lcdio_write_byte(0x00);
    lcdio_write_byte(0x01);   // 1
    lcdio_write_byte(0x00);
    lcdio_write_byte(0xA0);   // 160
    lcdio_write_reg(0x2C);
}

/**
 * @brief 在指定区域填充颜色
 *
 * @param x_start 起始列坐标
 * @param y_start 起始行坐标
 * @param x_end 终止列坐标
 * @param y_end 终止行坐标
 * @param color 要填充的颜色
 */
void lcd_fill(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint16_t i, j;
    lcdio_address_set(x_start, y_start, x_end - 1, y_end - 1); // 设置显示范围
    for (i = y_start; i < y_end; i++)
    {
        for (j = x_start; j < x_end; j++)
        {
            lcdio_write_halfword(color);
        }
    }
}

/**
 * @brief 在指定位置画点
 *
 * @param x 画点的X坐标
 * @param y 画点的Y坐标
 * @param color 点的颜色
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcdio_address_set(x, y, x, y); // 设置光标位置
    lcdio_write_halfword(color);
}

/**
 * @brief 画线
 *
 * @param x_start 起始X坐标
 * @param y_start 起始Y坐标
 * @param x_end 终止X坐标
 * @param y_end 终止Y坐标
 * @param color 线的颜色
 */
void lcd_draw_line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x_end - x_start; // 计算坐标增量
    delta_y = y_end - y_start;
    uRow = x_start; // 画线起点坐标
    uCol = y_start;
    if (delta_x > 0) incx = 1; // 设置单步方向
    else if (delta_x == 0) incx = 0; // 垂直线
    else { incx = -1; delta_x = -delta_x; }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0; // 水平线
    else { incy = -1; delta_y = -delta_y; }
    if (delta_x > delta_y) distance = delta_x; // 选取基本增量坐标轴
    else distance = delta_y;
    for (t = 0; t < distance + 1; t++)
    {
        lcd_draw_point(uRow, uCol, color); // 画点
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/**
 * @brief 画矩形
 *
 * @param x_start 起始X坐标
 * @param y_start 起始Y坐标
 * @param x_end 终止X坐标
 * @param y_end 终止Y坐标
 * @param color 矩形的颜色
 */
void lcd_draw_rectangle(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    lcd_draw_line(x_start, y_start, x_end, y_start, color);
    lcd_draw_line(x_start, y_start, x_start, y_end, color);
    lcd_draw_line(x_start, y_end, x_end, y_end, color);
    lcd_draw_line(x_end, y_start, x_end, y_end, color);
}

/**
 * @brief 画圆
 *
 * @param x_center 圆心的X坐标
 * @param y_center 圆心的Y坐标
 * @param radius 圆的半径
 * @param color 圆的颜色
 */
void lcd_draw_circle(uint16_t x_center, uint16_t y_center, uint8_t radius, uint16_t color)
{
    int a, b;
    a = 0;
    b = radius;
    while (a <= b)
    {
        lcd_draw_point(x_center - b, y_center - a, color); // 3
        lcd_draw_point(x_center + b, y_center - a, color); // 0
        lcd_draw_point(x_center - a, y_center + b, color); // 1
        lcd_draw_point(x_center - a, y_center - b, color); // 2
        lcd_draw_point(x_center + b, y_center + a, color); // 4
        lcd_draw_point(x_center + a, y_center - b, color); // 5
        lcd_draw_point(x_center + a, y_center + b, color); // 6
        lcd_draw_point(x_center - b, y_center + a, color); // 7
        a++;
        if ((a * a + b * b) > (radius * radius)) // 判断要画的点是否过远
        {
            b--;
        }
    }
}

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
void lcd_show_char(uint16_t x, uint16_t y, uint8_t character, uint16_t foreground_color, uint16_t background_color, uint8_t font_size, uint8_t mode)
{
    uint8_t temp, char_width, t, m = 0;
    uint16_t i, typeface_size; // 一个字符所占字节大小
    uint16_t x0 = x;
    char_width = font_size / 2;
    typeface_size = (char_width / 8 + ((char_width % 8) ? 1 : 0)) * font_size;
    character = character - ' ';    // 得到偏移后的值
    lcdio_address_set(x, y, x + char_width - 1, y + font_size - 1);  // 设置光标位置
    for (i = 0; i < typeface_size; i++)
    {
        if (font_size == FONTSIZE_1206) temp = ascii_1206[character][i];        // 调用6x12字体
        else if (font_size == FONTSIZE_1608) temp = ascii_1608[character][i];   // 调用8x16字体
        else if (font_size == FONTSIZE_2412) temp = ascii_2412[character][i];   // 调用12x24字体
        else if (font_size == FONTSIZE_3216) temp = ascii_3216[character][i];   // 调用16x32字体
        else return;
        for (t = 0; t < 8; t++)
        {
            if (!mode) // 非叠加模式
            {
                if (temp & (0x01 << t)) lcdio_write_halfword(foreground_color);
                else lcdio_write_halfword(background_color);
                m++;
                if (m % char_width == 0)
                {
                    m = 0;
                    break;
                }
            }
            else // 叠加模式
            {
                if (temp & (0x01 << t)) lcd_draw_point(x, y, foreground_color); // 画一个点
                x++;
                if ((x - x0) == char_width)
                {
                    x = x0;
                    y++;
                    break;
                }
            }
        }
    }
}

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
void lcd_show_string(uint16_t x, uint16_t y, const uint8_t *str, uint16_t foreground_color, uint16_t background_color, uint8_t font_size, uint8_t mode)
{
    while (*str != '\0')
    {
        lcd_show_char(x, y, *str, foreground_color, background_color, font_size, mode);
        x += font_size / 2;
        str++;
    }
}

/**
 * @brief 显示图片
 *
 * @param x 起点X坐标
 * @param y 起点Y坐标
 * @param length 图片长度
 * @param width 图片宽度
 * @param pic 图片数组
 */
void lcd_show_picture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[])
{
    uint16_t i, j;
    uint32_t k = 0;
    lcdio_address_set(x, y, x + length - 1, y + width - 1);
    for (i = 0; i < length; i++)
    {
        for (j = 0; j < width; j++)
        {
            lcdio_write_byte(pic[k * 2]);
            lcdio_write_byte(pic[k * 2 + 1]);
            k++;
        }
    }
}
