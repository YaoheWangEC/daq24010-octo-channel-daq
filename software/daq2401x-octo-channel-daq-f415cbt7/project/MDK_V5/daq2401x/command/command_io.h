#ifndef COMMAND_IO_H
#define COMMAND_IO_H

#include <stdint.h>
#include <stdbool.h>

#define RX_BUFFER_LENGTH_BYTE (64 * 2)  /**< 命令接收拼接缓冲区长度 */

typedef struct command_io
{
    volatile bool available;                                /**< 是否可用 */

    char rx_buffer[RX_BUFFER_LENGTH_BYTE];                  /**< 命令接收缓冲区 */
    volatile uint32_t rx_length;                            /**< 命令接收长度 */

    bool (*init)(void);                                     /**< 初始化 */
    void (*rx_buffer_flush)(void);                          /**< 清空命令接收缓冲区 */
    void (*rx_handler)(const uint8_t * buf, uint32_t len);  /**< 接收中断handler */
    bool (*tx_start)(const uint8_t * buf, uint32_t len);    /**< 开始一次分段发送 */
    void (*tx_handler)(void);                               /**< 发送中断handler */
    bool (*tx_busy)(void);                                  /**< 是否正在发送 */
    bool (*tx_continue)(void);                              /**< 继续下一片段发送 */
    bool (*tx_idle)(void);                                  /**< 是否完成一轮发送 */
} command_io_t;

// extern command_io_t command_io_usb;   // USB CDC 实例（本工程未编入，USB 驱动未接入）
extern command_io_t command_io_uart;     // UART 实例（USART2 + RS485）

/**
 * @brief  USART 接收数据处理（由 USART2 IDLE ISR 清除标志后调用）
 * @note   内部封装 DMA 计数读取、rx_handler 拼包与 DMA 重配，
 *         中断服务函数无需感知内部缓冲与 DMA 通道。
 */
void command_io_uart_rx_isr(void);

/**
 * @brief  初始化 command_io_t 实例
 * 
 * @param  io  指向 command_io_t 实例
 */
void command_io_init(command_io_t *io);

/**
 * @brief  从接收缓冲区中提取完整命令并压入 FIFO
 * @param  io  指向 command_io 实例
 *
 * 扫描 rx_buffer 中的 \r\n 分隔符, 找到后将命令字符串压入 FIFO,
 * 并移除已消费数据。支持粘包 (一条扫描出多条命令) 和拆包 (
 * 未找到分隔符时保留数据等待下一包)。
 *
 * @note   在 ISR 上下文 (rx_handler) 中调用。
 */
void command_io_fetch_cmd(command_io_t *io);

#endif
