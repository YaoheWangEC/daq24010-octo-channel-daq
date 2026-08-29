#include "lcd_io.h"
#include "lcd.h"
#include "platform.h"

#if (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
#include "at32f415_wk_config.h"
#include "at32f415_spi.h"
#include "at32f415_gpio.h"
#include "wk_system.h"
#include "wk_dma.h"
#elif (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
// 预留位置
#endif

static write_mode_t curr_write_mode;

/**
 * @brief 初始化 LCD IO 接口
 *
 * 设置默认写入模式为数据模式，准备好后续的寄存器和数据传输。
 */
void lcdio_init(void)
{
    lcdio_write_mode(WRITE_DATA);
}

/**
 * @brief 控制 LCD 硬件复位
 *
 * @param enable true 表示拉低 RESET 引脚进行复位，false 表示释放复位。
 */
void lcdio_reset(bool enable)
{
    if (enable)
    {
        gpio_bits_reset(LCD_RESET_GPIO_PORT, LCD_RESET_PIN);
    }
    else
    {
        gpio_bits_set(LCD_RESET_GPIO_PORT, LCD_RESET_PIN);
    }
}

/**
 * @brief 延时函数
 *
 * @param delay_ms 延时时间，单位毫秒。
 */
void lcdio_delay_ms(uint32_t delay_ms)
{
    wk_delay_ms(delay_ms);
}

/**
 * @brief 切换写入模式
 *
 * 控制 DC 引脚以区分命令和数据写入。
 *
 * @param write_mode WRITE_CMD 表示写命令，WRITE_DATA 表示写数据。
 */
void lcdio_write_mode(write_mode_t write_mode)
{
    if ((curr_write_mode != WRITE_CMD) && (write_mode == WRITE_CMD)) // 写命令时DC拉低
    {
        gpio_bits_reset(LCD_DC_GPIO_PORT, LCD_DC_PIN);
        curr_write_mode = WRITE_CMD;
    }
    else if ((curr_write_mode != WRITE_DATA) && (write_mode == WRITE_DATA))// 写数据时DC拉高
    {
        gpio_bits_set(LCD_DC_GPIO_PORT, LCD_DC_PIN);
        curr_write_mode = WRITE_DATA;
    }
}

/**
 * @brief 写入一个字节数据
 *
 * 在数据模式下，通过 SPI 向 LCD 发送一个字节。
 *
 * @param data 要写入的 8 位数据。
 */
void lcdio_write_byte(uint8_t data)
{
    lcdio_write_mode(WRITE_DATA);
    gpio_bits_reset(LCD_CS_GPIO_PORT, LCD_CS_PIN);
    spi_i2s_data_transmit(LCD_SPI, data);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_BF_FLAG) == SET);
    gpio_bits_set(LCD_CS_GPIO_PORT, LCD_CS_PIN);
}

/**
 * @brief 写入一个半字数据
 *
 * 在数据模式下，通过 SPI 向 LCD 发送一个 16 位数据（RGB565 等）。
 *
 * @param data 要写入的 16 位数据。
 */
void lcdio_write_halfword(uint16_t data)
{
    lcdio_write_mode(WRITE_DATA);
    gpio_bits_reset(LCD_CS_GPIO_PORT, LCD_CS_PIN);
    spi_i2s_data_transmit(LCD_SPI, data >> 8);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    spi_i2s_data_transmit(LCD_SPI, data);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_BF_FLAG) == SET);
    gpio_bits_set(LCD_CS_GPIO_PORT, LCD_CS_PIN);
}

/**
 * @brief 写入一个寄存器命令
 *
 * 在命令模式下，通过 SPI 向 LCD 发送一个寄存器地址。
 *
 * @param reg 寄存器地址。
 */
void lcdio_write_reg(uint8_t reg)
{
    lcdio_write_mode(WRITE_CMD);
    gpio_bits_reset(LCD_CS_GPIO_PORT, LCD_CS_PIN);
    spi_i2s_data_transmit(LCD_SPI, reg);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_BF_FLAG) == SET);
    gpio_bits_set(LCD_CS_GPIO_PORT, LCD_CS_PIN);
    lcdio_write_mode(WRITE_DATA);
}

/**
 * @brief 设置显示区域
 *
 * 根据屏幕方向设置列地址和行地址范围。
 *
 * @param x_start 起始列坐标
 * @param y_start 起始行坐标
 * @param x_end   终止列坐标
 * @param y_end   终止行坐标
 */
void lcdio_address_set(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    if (USE_HORIZONTAL == 0)
    {
        lcdio_write_reg(0x2a); // 列地址设置
        lcdio_write_halfword(x_start + 26);
        lcdio_write_halfword(x_end + 26);
        lcdio_write_reg(0x2b); // 行地址设置
        lcdio_write_halfword(y_start + 1);
        lcdio_write_halfword(y_end + 1);
        lcdio_write_reg(0x2c); // 储存器写
    }
    else if (USE_HORIZONTAL == 1)
    {
        lcdio_write_reg(0x2a); // 列地址设置
        lcdio_write_halfword(x_start + 26);
        lcdio_write_halfword(x_end + 26);
        lcdio_write_reg(0x2b); // 行地址设置
        lcdio_write_halfword(y_start + 1);
        lcdio_write_halfword(y_end + 1);
        lcdio_write_reg(0x2c); // 储存器写
    }
    else if (USE_HORIZONTAL == 2)
    {
        lcdio_write_reg(0x2a); // 列地址设置
        lcdio_write_halfword(x_start + 1);
        lcdio_write_halfword(x_end + 1);
        lcdio_write_reg(0x2b); // 行地址设置
        lcdio_write_halfword(y_start + 26);
        lcdio_write_halfword(y_end + 26);
        lcdio_write_reg(0x2c); // 储存器写
    }
    else
    {
        lcdio_write_reg(0x2a); // 列地址设置
        lcdio_write_halfword(x_start + 1);
        lcdio_write_halfword(x_end + 1);
        lcdio_write_reg(0x2b); // 行地址设置
        lcdio_write_halfword(y_start + 26);
        lcdio_write_halfword(y_end + 26);
        lcdio_write_reg(0x2c); // 储存器写
    }
}

/**
 * @brief 启动一次批传输，将缓冲区数据发送到 LCD
 *
 * 使用 DMA 将指定缓冲区的数据批量传输到 LCD。
 *
 * @param buf          数据缓冲区指针
 * @param buf_len_byte 缓冲区长度（字节数）
 */
void lcdio_transmit_buffer(uint8_t *buf, uint16_t buf_len_byte)
{
    // 关闭通道，配置参数
    dma_channel_enable(LCD_DMA_CHANNEL, FALSE);
    wk_dma_channel_config(LCD_DMA_CHANNEL,
                          (uint32_t)&(LCD_SPI->dt), // 外设数据寄存器地址
                          (uint32_t)buf,         // 内存缓冲区地址
                          buf_len_byte);         // 传输字节数

    // 清除完成标志
    dma_flag_clear(LCD_DMA_FLAG_FDT | LCD_DMA_FLAG_HDT | LCD_DMA_FLAG_DTERR);

    // 拉低CS
    gpio_bits_reset(LCD_CS_GPIO_PORT, LCD_CS_PIN);

    // 启动 DMA 通道
    dma_channel_enable(LCD_DMA_CHANNEL, TRUE);

    // 启动 SPI 的 DMA 发送
    spi_i2s_dma_transmitter_enable(LCD_SPI, TRUE);
}

/**
 * @brief 查询批传输是否完成
 *
 * @return true 表示 DMA 已完成传输，false 表示仍在传输中。
 */
bool lcdio_interface_is_free(void)
{
    return (dma_flag_get(LCD_DMA_FLAG_FDT) != RESET);
}

/**
 * @brief 等待批传输完成（阻塞）
 *
 * 阻塞等待 DMA 完成，并确保 SPI 总线空闲，最后释放 CS 引脚。
 */
void lcdio_wait_interface_free(void)
{
    // 等待DMA全传输完成
    while(dma_flag_get(LCD_DMA_FLAG_FDT) == RESET);

    // 清除完成标志
    dma_flag_clear(LCD_DMA_FLAG_FDT);

    // 关闭DMA通道，避免误触发
    dma_channel_enable(LCD_DMA_CHANNEL, FALSE);

    // 等待SPI总线空闲，确保最后一个字节发出
    while(spi_i2s_flag_get(LCD_SPI, SPI_I2S_BF_FLAG) == SET);

    // 拉高CS
    gpio_bits_set(LCD_CS_GPIO_PORT, LCD_CS_PIN);
}

