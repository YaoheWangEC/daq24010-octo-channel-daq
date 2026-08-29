/**
 * @file lcd_io.h
 * @brief LCD 底层 IO 接口说明
 *
 * 本模块提供 LCD 的底层硬件接口封装，负责 GPIO、SPI 以及 DMA 的操作，
 * 上层绘图函数通过调用这些接口完成数据传输和寄存器配置。
 * 通过抽象接口，屏蔽了具体的硬件细节，提升了代码的可移植性和可维护性。
 *
 * 使用步骤：
 * 1. 调用 `lcdio_init` 初始化接口状态。
 * 2. 根据需要调用 `lcdio_reset` 控制 LCD 的硬件复位。
 * 3. 使用 `lcdio_write_reg`、`lcdio_write_byte`、`lcdio_write_halfword`
 *    完成寄存器命令和数据的写入。
 * 4. 使用 `lcdio_address_set` 设置显示区域。
 * 5. 对于大块数据传输，调用 `lcdio_transmit_buffer` 启动 DMA 传输，
 *    并通过 `lcdio_interface_is_free` 或 `lcdio_wait_interface_free` 查询/等待传输完成。
 *
 * 注意事项：
 * - 本模块依赖于 SPI 外设和对应的 SPI_TX DMA 通道，请确保在系统初始化时正确配置。
 * - DC 脚用于区分命令和数据，CS 脚用于片选控制，RESET 脚用于硬件复位。
 * - `lcdio_write_mode` 用于切换命令/数据模式，内部维护当前状态避免重复操作。
 * - 在 DMA 模式下，推荐使用 `lcdio_wait_interface_free` 确保传输完成并释放总线。
 * - 不建议在没有启动批传输时调用 `lcdio_wait_interface_free` 查询状态。
 *
 * 提供的主要接口函数：
 * - `lcdio_init`：初始化 LCD IO 接口。
 * - `lcdio_reset`：控制 LCD 硬件复位。
 * - `lcdio_delay_ms`：延时函数。
 * - `lcdio_write_mode`：切换命令/数据写入模式。
 * - `lcdio_write_byte`：写入一个字节数据。
 * - `lcdio_write_halfword`：写入一个 16 位数据。
 * - `lcdio_write_reg`：写入一个寄存器命令。
 * - `lcdio_address_set`：设置显示区域。
 * - `lcdio_transmit_buffer`：启动 DMA 批量传输。
 * - `lcdio_interface_is_free`：查询 DMA 是否完成。
 * - `lcdio_wait_interface_free`：阻塞等待 DMA 完成。
 *
 * 通过这些接口，上层 `lcd.c` 和 `colorlut.c` 可以方便地实现图形绘制和刷新，
 * 而无需关心底层 SPI/DMA 的具体实现。
 */

#ifndef LCD_IO_H
#define LCD_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WRITE_DATA = 1,
    WRITE_CMD = 0
} write_mode_t;

#if (PLATFORM_DEVICE == DEVICE_AT32) && (PLATFORM_DRIVER == DRIVER_SPL)
// SPI 外设选择
#define LCD_SPI                SPI2

// DMA 通道选择
#define LCD_DMA_CHANNEL        DMA1_CHANNEL5

// DMA 完成标志
#define LCD_DMA_FLAG_FDT       DMA1_FDT5_FLAG
#define LCD_DMA_FLAG_HDT       DMA1_HDT5_FLAG
#define LCD_DMA_FLAG_DTERR     DMA1_DTERR5_FLAG

// GPIO 片选、数据/命令、复位引脚定义在 at32f415_wk_config.h

#elif (PLATFORM_DEVICE == DEVICE_STM32) && (PLATFORM_DRIVER == DRIVER_HAL)
// 预留位置
#endif

/**
 * @brief 初始化 LCD IO 接口
 *
 * 设置默认写入模式为数据模式，准备好后续的寄存器和数据传输。
 */
void lcdio_init(void);

/**
 * @brief 控制 LCD 硬件复位
 *
 * @param enable true 表示拉低 RESET 引脚进行复位，false 表示释放复位。
 */
void lcdio_reset(bool enable);

/**
 * @brief 延时函数
 *
 * @param delay_ms 延时时间，单位毫秒。
 */
void lcdio_delay_ms(uint32_t delay_ms);

/**
 * @brief 切换写入模式
 *
 * 控制 DC 引脚以区分命令和数据写入。
 *
 * @param write_mode WRITE_CMD 表示写命令，WRITE_DATA 表示写数据。
 */
void lcdio_write_mode(write_mode_t write_mode);

/**
 * @brief 写入一个字节数据
 *
 * 在数据模式下，通过 SPI 向 LCD 发送一个字节。
 *
 * @param data 要写入的 8 位数据。
 */
void lcdio_write_byte(uint8_t data);

/**
 * @brief 写入一个半字数据
 *
 * 在数据模式下，通过 SPI 向 LCD 发送一个 16 位数据（RGB565 等）。
 *
 * @param data 要写入的 16 位数据。
 */
void lcdio_write_halfword(uint16_t data);

/**
 * @brief 写入一个寄存器命令
 *
 * 在命令模式下，通过 SPI 向 LCD 发送一个寄存器地址。
 *
 * @param reg 寄存器地址。
 */
void lcdio_write_reg(uint8_t reg);

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
void lcdio_address_set(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);

/**
 * @brief 启动一次批传输，将缓冲区数据发送到 LCD
 *
 * 使用 DMA 将指定缓冲区的数据批量传输到 LCD。
 *
 * @param buf          数据缓冲区指针
 * @param buf_len_byte 缓冲区长度（字节数）
 */
void lcdio_transmit_buffer(uint8_t *buf, uint16_t buf_len_byte);

/**
 * @brief 查询批传输是否完成
 *
 * @return true 表示 DMA 已完成传输，false 表示仍在传输中。
 */
bool lcdio_interface_is_free(void);

/**
 * @brief 等待批传输完成（阻塞）
 *
 * 阻塞等待 DMA 完成，并确保 SPI 总线空闲，最后释放 CS 引脚。
 */
void lcdio_wait_interface_free(void);

#ifdef __cplusplus
}
#endif

#endif // LCD_IO_H
