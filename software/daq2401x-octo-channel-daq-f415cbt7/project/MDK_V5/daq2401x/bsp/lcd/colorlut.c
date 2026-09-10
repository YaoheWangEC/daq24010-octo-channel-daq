#include "colorlut.h"
#include "lcd_io.h"
#include "platform.h"
#include "lcdfont.h"
#include <stdio.h>

#if (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#include "at32f415_wk_config.h"
#include "at32f415_gpio.h"
#include "wk_system.h"
#include "at32f415_spi.h"
#elif (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
// 预留位置
#endif

// SPI DMA缓冲区（每像素2字节），行扫描模式下大小 = 宽度
static uint16_t spi_buf[COLORLUT_AREA_WIDTH];

// 调色板存储数组
static uint16_t colorlut_palette[COLORLUT_MAX_COLORS];
static size_t colorlut_count = 0;

/**
 * @brief 初始化调色板
 *
 * 将用户提供的 RGB565 颜色数组复制到内部调色板。
 * 如果数量超过最大支持值，则自动截断。
 *
 * @param colors RGB565颜色数组
 * @param count  调色板颜色数量 (最大16或256；16 已实测，256 未验证)
 */
void colorlut_init(const uint16_t *colors, size_t count)
{
    if (count > COLORLUT_MAX_COLORS)
        count = COLORLUT_MAX_COLORS;

    for (size_t i = 0; i < count; i++)
    {
        colorlut_palette[i] = colors[i];
    }

    colorlut_count = count;
}

/**
 * @brief 获取调色板中的颜色
 *
 * 根据索引返回对应的 RGB565 颜色。
 * 如果索引越界，则返回黑色 (0x0000)。
 *
 * @param index 调色板索引
 * @return uint16_t RGB565颜色
 */
uint16_t colorlut_get_color(uint8_t index)
{
    if (index >= colorlut_count)
    {
        return 0x0000; // 默认返回黑色
    }
    return colorlut_palette[index];
}

/**
 * @brief 设置调色板中的某个颜色
 *
 * 修改指定索引的颜色值。如果索引超过当前调色板大小，
 * 会自动扩展调色板大小。
 *
 * @param index 调色板索引
 * @param color RGB565颜色
 */
void colorlut_set_color(uint8_t index, uint16_t color)
{
    if (index < COLORLUT_MAX_COLORS)
    {
        colorlut_palette[index] = color;
        if (index >= colorlut_count)
        {
            colorlut_count = index + 1;
        }
    }
}

/**
 * @brief 清空索引缓冲区
 *
 * 将整个缓冲区填充为指定的颜色索引。
 * 在 4bit 模式下，每字节包含两个像素索引；
 * 在 8bit 模式下，每字节包含一个像素索引。
 *
 * @param buf   索引缓冲区指针
 * @param width 图像宽度
 * @param height图像高度
 * @param color_index 填充颜色索引
 */
void colorlut_clear_buffer(uint8_t *buf, size_t width, size_t height, uint8_t color_index)
{
#if COLORLUT_MAX_COLORS <= 16
    // 4bit模式：每字节两个像素
    uint8_t packed = (color_index & 0x0F) | ((color_index & 0x0F) << 4);
    size_t bytes = (width * height + 1) / 2; // 向上取整
    for (size_t i = 0; i < bytes; i++)
    {
        buf[i] = packed;
    }
#elif COLORLUT_MAX_COLORS <= 256
    // 8bit模式：每字节一个像素
    size_t pixels = width * height;
    for (size_t i = 0; i < pixels; i++)
    {
        buf[i] = color_index;
    }
#else
    #error "Unsupported COLORLUT_MAX_COLORS value"
#endif
}

/**
 * @brief 设置某个像素的索引值
 *
 * 根据坐标计算缓冲区位置，并写入调色板索引。
 *
 * @param buf 索引缓冲区指针
 * @param x   像素X坐标
 * @param y   像素Y坐标
 * @param color_index 调色板索引
 */
void colorlut_set_pixel(uint8_t *buf, size_t x, size_t y, uint8_t color_index)
{
#if COLORLUT_MAX_COLORS <= 16
    size_t pixel_index = y * COLORLUT_AREA_WIDTH + x; // 行距 = COLORLUT_AREA_WIDTH（缓冲宽度）
    size_t byte_index = pixel_index / 2;
    if ((pixel_index & 1) == 0)
    {
        // 低4bit
        buf[byte_index] = (buf[byte_index] & 0xF0) | (color_index & 0x0F);
    }
    else
    {
        // 高4bit
        buf[byte_index] = (buf[byte_index] & 0x0F) | ((color_index & 0x0F) << 4);
    }
#elif COLORLUT_MAX_COLORS <= 256
    size_t pixel_index = y * COLORLUT_AREA_WIDTH + x;
    buf[pixel_index] = color_index;
#endif
}

/**
 * @brief 获取某个像素的索引值
 *
 * 根据坐标计算缓冲区位置，并返回调色板索引。
 *
 * @param buf 索引缓冲区指针
 * @param x   像素X坐标
 * @param y   像素Y坐标
 * @return uint8_t 调色板索引
 */
uint8_t colorlut_get_pixel(const uint8_t *buf, size_t x, size_t y)
{
#if COLORLUT_MAX_COLORS <= 16
    size_t pixel_index = y * COLORLUT_AREA_WIDTH + x;
    size_t byte_index = pixel_index / 2;
    if ((pixel_index & 1) == 0)
    {
        return buf[byte_index] & 0x0F;
    }
    else
    {
        return (buf[byte_index] >> 4) & 0x0F;
    }
#elif COLORLUT_MAX_COLORS <= 256
    size_t pixel_index = y * COLORLUT_AREA_WIDTH + x;
    return buf[pixel_index];
#endif
}

/**
 * @brief 绘制直线
 *
 * 使用 Bresenham 算法在索引缓冲区中绘制一条直线。
 * 算法通过逐点设置像素索引实现，支持任意斜率。
 *
 * @param buf 索引缓冲区
 * @param x0  起点X
 * @param y0  起点Y
 * @param x1  终点X
 * @param y1  终点Y
 * @param color_index 调色板索引
 */
void colorlut_draw_line(uint8_t *buf, size_t x0, size_t y0, size_t x1, size_t y1, uint8_t color_index)
{
    int dx = (int)x1 - (int)x0;
    int dy = (int)y1 - (int)y0;
    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;
    dx = (dx >= 0) ? dx : -dx;
    dy = (dy >= 0) ? dy : -dy;

    int err = dx - dy;

    while (1)
    {
        colorlut_set_pixel(buf, x0, y0, color_index);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief 绘制空心矩形
 *
 * 在索引缓冲区中绘制一个矩形的边框。
 * 通过调用直线绘制函数绘制四条边。
 *
 * @param buf 索引缓冲区
 * @param x   左上角X
 * @param y   左上角Y
 * @param w   宽度
 * @param h   高度
 * @param color_index 调色板索引
 */
void colorlut_draw_rect(uint8_t *buf, size_t x, size_t y, size_t w, size_t h, uint8_t color_index)
{
    size_t x1 = x + w - 1;
    size_t y1 = y + h - 1;

    // 上边
    colorlut_draw_line(buf, x, y, x1, y, color_index);
    // 下边
    colorlut_draw_line(buf, x, y1, x1, y1, color_index);
    // 左边
    colorlut_draw_line(buf, x, y, x, y1, color_index);
    // 右边
    colorlut_draw_line(buf, x1, y, x1, y1, color_index);
}

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
void colorlut_show_char(uint8_t *buf, uint16_t x, uint16_t y, uint8_t character, uint8_t color_index, uint8_t font_size)
{
    uint8_t temp, char_width, t;
    uint16_t i, typeface_size;
    uint16_t x0 = x;

    char_width = font_size / 2;
    typeface_size = (char_width / 8 + ((char_width % 8) ? 1 : 0)) * font_size;
    character = character - ' '; // 得到偏移后的值

    for (i = 0; i < typeface_size; i++)
    {
        if (font_size == FONTSIZE_1206) temp = ascii_1206[character][i];
        else if (font_size == FONTSIZE_1608) temp = ascii_1608[character][i];
        else if (font_size == FONTSIZE_2412) temp = ascii_2412[character][i];
        else if (font_size == FONTSIZE_3216) temp = ascii_3216[character][i];
        else return;

        for (t = 0; t < 8; t++)
        {
            if (temp & (0x01 << t))
            {
                // 在缓冲区中绘制一个点（叠加模式，只设置前景色）
                colorlut_set_pixel(buf, x, y, color_index);
            }
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
void colorlut_show_string(uint8_t *buf, uint16_t x, uint16_t y, const uint8_t *str, uint8_t color_index, uint8_t font_size)
{
    while (*str != '\0')
    {
        colorlut_show_char(buf, x, y, *str, color_index, font_size);
        x += font_size / 2; // 字符宽度
        str++;
    }
}

/**
 * @brief 将索引缓冲区内容刷新到LCD显示器
 *
 * 本函数把一块"自包含"的索引缓冲区贴到 LCD 的指定位置。
 * 缓冲区采用自身局部坐标（原点为缓冲左上角，行距 = COLORLUT_AREA_WIDTH），
 * x_start/y_start 仅表示该区域贴在屏幕上的目标位置，不参与缓冲寻址。
 * 因此缓冲宽度无须等于屏宽，可为任意的局部区域缓冲。
 *
 * @param src     索引缓冲区指针（内容按缓冲局部坐标绘制，原点为缓冲左上角）
 * @param x_start 该区域在屏幕上的目标起始X坐标
 * @param y_start 该区域在屏幕上的目标起始Y坐标
 * @param width   该区域的宽度（像素数，<= COLORLUT_AREA_WIDTH）
 * @param height  该区域的高度（像素数）
 *
 * @note 使用前需确保LCD已初始化，并且SPI DMA可用。
 *       全屏整刷可传 (0,0,LCD_W,LCD_H)（此时缓冲即整屏，局部坐标=屏幕坐标）。
 */
void colorlut_flush_to_lcd(const uint8_t *src,
    size_t x_start,
    size_t y_start,
    size_t width,
    size_t height)
{
    // 设置显示区域
    lcdio_address_set(x_start, y_start, x_start + width - 1, y_start + height - 1);

    // 发送数据模式
    lcdio_write_mode(WRITE_DATA);

    bool is_first_line = true;

    // 行扫描（逐行）：数据序与 MADCTL 地址递增方向一致
    for (size_t y = 0; y < height; y += 1)
    {
        // 等待上一次 DMA 完成
        if (is_first_line)
        {
            is_first_line = false;
        }
        else
        {
            // lcdio_wait_interface_free();
            while(lcdio_interface_is_free() == false);
        }

        // 展开一行数据到缓冲区，保证逐行逐像素顺序（按缓冲内局部坐标读）
        uint16_t buf_index = 0;
        for (size_t x = 0; x < width; x++)
        {
            uint8_t color_index = colorlut_get_pixel(src, x, y);
            uint16_t color = colorlut_get_color(color_index);
            spi_buf[buf_index] = (color >> 8) | (color << 8); // 高低字节交换
            buf_index++;
        }

        // 启动 DMA 传输（每行 width 像素 × 2字节）
        lcdio_transmit_buffer((uint8_t*)spi_buf, width * 2);

    }
    // 等待最后一次 DMA 完成
    lcdio_wait_interface_free();
}

/**
 * @brief 异步刷屏（无栈状态机 + pingpong 缓冲，非阻塞）
 *
 * 每次调用推进一次状态机：返回 0 表示空闲（本次入参作为新帧开始刷屏），
 * 返回非 0 表示刷屏进行中（本次入参被忽略，仅推进当前帧）。
 * 适用于主循环周期驱动，避免长时间阻塞。
 *
 * @param src     索引缓冲区指针（内容按缓冲局部坐标绘制，原点为缓冲左上角）
 * @param x_start 该区域在屏幕上的目标起始X坐标
 * @param y_start 该区域在屏幕上的目标起始Y坐标
 * @param width   该区域的宽度（像素数，<= COLORLUT_AREA_WIDTH）
 * @param height  该区域的高度（像素数）
 * @retval 0=空闲可接收新帧；非0=刷屏进行中
 */
int colorlut_flush_to_lcd_async(const uint8_t *src,
    size_t x_start,
    size_t y_start,
    size_t width,
    size_t height)
{
    static int state = 0; // 0-空闲 1-发送已加载缓冲 2-加载下一行到另一缓冲 3-等待DMA完成
    static int line_cplt = 0; // 已传输完成行数
    static bool tx_buf_index = false; // DMA 当前发送的缓冲索引（false=ping true=pong）
    static uint16_t spi_buf_ping[sizeof(spi_buf) / sizeof(uint16_t)];   // ping 独立分配
    static uint16_t *spi_buf_pong = spi_buf;                            // pong 借用全局 spi_buf（阻塞/异步刷屏互斥）

    // 当state空闲时保存所有入参，后续刷屏使用暂存的参数进行刷屏
    static const uint8_t *tmp_src;
    static size_t tmp_x_start;
    static size_t tmp_y_start;
    static size_t tmp_width;
    static size_t tmp_height;

    if (state == 0)
    {
        // 保存现场
        tmp_src = src;
        tmp_x_start = x_start;
        tmp_y_start = y_start;
        tmp_width = width;
        tmp_height = height;

        // 初始化
        line_cplt = 0;
        tx_buf_index = false;

        // 设置显示区域
        lcdio_address_set(tmp_x_start, tmp_y_start, tmp_x_start + tmp_width - 1, tmp_y_start + tmp_height - 1);

        // 发送数据模式
        lcdio_write_mode(WRITE_DATA);

        // 加载 line0 到 ping 缓冲并启动传输（按缓冲内局部坐标读）
        for (size_t x = 0; x < tmp_width; x++)
        {
            uint8_t ci = colorlut_get_pixel(tmp_src, x, 0);
            uint16_t c = colorlut_get_color(ci);
            spi_buf_ping[x] = (uint16_t)((c >> 8) | (c << 8)); // 高低字节交换
        }
        lcdio_transmit_buffer((uint8_t *)spi_buf_ping, tmp_width * 2);

        // 进入加载状态：填充 line1 到 pong 缓冲，与 line0 传输重叠
        state = 2;
    }
    else if (state == 2)
    {
        // 加载下一行到另一缓冲（DMA 正在发送 tx_buf_index 指向的缓冲；按缓冲内局部坐标读）
        int next_row = line_cplt + 1;
        if (next_row < (int)tmp_height)
        {
            uint16_t *load_buf = (tx_buf_index == false) ? spi_buf_pong : spi_buf_ping;   // 加载到非发送缓冲
            for (size_t x = 0; x < tmp_width; x++)
            {
                uint8_t ci = colorlut_get_pixel(tmp_src, x, next_row);
                uint16_t c = colorlut_get_color(ci);
                load_buf[x] = (uint16_t)((c >> 8) | (c << 8)); // 高低字节交换
            }
        }
        state = 3;
    }
    else if (state == 3)
    {
        // 等待当前行 DMA 完成（行间仅需 FDT，无需等 SPI 清空）
        if (lcdio_interface_is_free() == true)
        {
            line_cplt++;
            if (line_cplt >= (int)tmp_height)
            {
                // 全部完成：收尾（清标志、关通道、等 SPI 空闲、CS 拉高）
                lcdio_wait_interface_free();
                state = 0;
            }
            else
            {
                tx_buf_index = !tx_buf_index;   // 切换到刚加载好的另一缓冲
                state = 1;
            }
        }
    }
    else if (state == 1)
    {
        // 发送刚加载的另一缓冲
        uint16_t *tx_ptr = (tx_buf_index == false) ? spi_buf_ping : spi_buf_pong;
        lcdio_transmit_buffer((uint8_t *)tx_ptr, tmp_width * 2);
        state = 2;
    }

    return state;
}
