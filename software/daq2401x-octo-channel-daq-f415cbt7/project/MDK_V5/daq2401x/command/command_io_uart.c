/**
 * @file    command_io_uart.c
 * @brief   UART 命令 I/O 驱动, 实现 command_io_t 接口
 *
 * 适用于半双工 RS485 / 全双工 RS422/RS232 / 标准 UART 接口。
 * RX: DMA normal 模式 + IDLE 中断, ISR 中调 rx_handler 拼包。
 * TX: DMA normal 模式 + TC 中断, 一次性全发 (无分片)。
 *
 * @section uart_config 端口与 DMA 配置
 *
 * 1. 根据实际硬件修改以下宏:
 *    - DMA_RX_CHANNEL / DMA_TX_CHANNEL: 串口使用的 DMA 通道。
 *    - IO_USART: USART 外设基地址 (USART1 / USART2 / ...)。
 *    - DMA_RX_BUF_SIZE: DMA 接收缓冲区大小, 建议不小于最长命令长度。
 *
 * 2. DMA 配置 (在项目初始化代码中完成):
 *    - RX 通道: 外设→内存, 字节宽度, normal 模式, 开启 USART IDLE 中断。
 *    - TX 通道: 内存→外设, 字节宽度, normal 模式, 开启 DMA TC 中断。
 *    - 两个通道在 uart_init() 中仅做启停, 基础配置由外部 wk_dma_channel_config 完成。
 *
 * 3. 中断注册 (在项目的 ISR 文件中):
 *    - USART IDLE ISR: 读取 DMA_CNDTR 计算接收长度, 调
 *      command_io_uart.rx_handler(rx_dma_buf, len), 重配 DMA 计数后重新启用。
 *      参考 uart_rx_handler() 函数内注释代码。
 *    - DMA TX TC ISR: 调 command_io_uart.tx_handler()。
 *
 * @section bus_mode 不同总线形式的适配
 *
 * uart_set_txrx_mode() 根据总线类型修改:
 *
 * | 总线     | TX 操作                             | RX 操作                             |
 * |----------|--------------------------------------|--------------------------------------|
 * | RS485    | DE 拉高 + USART TX 使能             | DE 拉低 + USART RX 使能             |
 * | RS422    | USART TX 使能                        | USART RX 使能                        |
 * | RS232    | USART TX 使能 (全双工, 收发可共存)   | USART RX 使能                        |
 * | UART TTL | USART TX 使能 (全双工, 收发可共存)   | USART RX 使能                        |
 *
 * 全双工模式下可删除 uart_set_txrx_mode() 中的互斥检查 (tx && rx 返回 false)。
 */

#include "command_io.h"

#include "at32f415_wk_config.h"
#include "at32f415_usart.h"
#include "wk_dma.h"

#include <string.h>

#define DMA_RX_CHANNEL          DMA1_CHANNEL1       /**< 串口接收 DMA 通道，Normal 模式（USART2_RX） */
#define DMA_TX_CHANNEL          DMA1_CHANNEL2       /**< 串口发送 DMA 通道，Normal 模式（USART2_TX） */
#define IO_USART                USART2              /**< 外设基地址 */

#define DMA_RX_BUF_SIZE         128         /**< DMA RX 缓冲区大小 */

static volatile bool tx_idle    = true;     /**< 一轮发送是否空闲 */
static volatile bool tx_busy    = false;    /**< DMA 发送是否进行中 */

static uint8_t rx_dma_buf[DMA_RX_BUF_SIZE];     /**< DMA RX 硬件缓冲区 */


/**
 * @brief 切换收发模式
 * 
 * @param tx 开启发送 
 * @param rx 开启接收
 * @return true 正常切换
 * @return false 切换错误
 * 
 * 切换UART的收发状态，设计用于 RS485 等半双工通信，使用TX/RX
 * 可独立使能发送接收。支持同时禁用 TX/RX 。
 */
static bool uart_set_txrx_mode(bool tx, bool rx)
{
    if ((tx == true) && (rx == true))
    {
        return false;
    }

    if (tx == true)
    {
        gpio_bits_set(RS485_DE_GPIO_PORT, RS485_DE_PIN);    // RS485 发送使能
    }

    if (rx == true)
    {
        gpio_bits_reset(RS485_DE_GPIO_PORT, RS485_DE_PIN);  // RS485 接收使能
    }

    return true;
}

/**
 * @brief  初始化 UART 命令 IO
 * @retval true  成功
 *
 * 配置 DMA RX (循环模式) + IDLE 中断, 默认接收状态。
 * ISR 需在外部注册: USART IDLE → 调用 uart_rx_isr(),
 * DMA_TX_CHANNEL TC → 调用 command_io_uart.tx_handler()。
 */
static bool uart_init(void)
{
    tx_idle = true;
    tx_busy = false;

    uart_set_txrx_mode(false, false); // 完全静音

    usart_dma_receiver_enable(IO_USART, TRUE);
    usart_dma_transmitter_enable(IO_USART, TRUE);

    dma_channel_enable(DMA_RX_CHANNEL, FALSE); // 禁用 USART_RX 的 DMA 接收
    usart_flag_clear(IO_USART, USART_IDLEF_FLAG); // 强制清除 IDLE 标志

    // 清空残留的接收数据
    while (usart_flag_get(IO_USART, USART_RDBF_FLAG) != RESET)
    {
        volatile uint32_t dummy = IO_USART->dt;
        (void)dummy; // 读出丢弃
    }

    wk_dma_channel_config(DMA_RX_CHANNEL,
        (uint32_t)&IO_USART->dt,
        (uint32_t)rx_dma_buf,
        sizeof(rx_dma_buf)); // 配置DMA接收通道

    dma_channel_enable(DMA_RX_CHANNEL, TRUE); // 打开 USART_RX 的 DMA 接收
    usart_interrupt_enable(IO_USART, USART_IDLE_INT, TRUE); // 使能 UART 空闲中断

    uart_set_txrx_mode(false, true); // 开始接收

    return true;
}

/**
 * @brief 清空命令接收缓冲
 * 
 */
static void uart_rx_buffer_flush(void)
{
    command_io_uart.rx_length = 0;
}

/**
 * @brief  接收处理 (由 USART IDLE ISR 或主循环触发)
 * @param  buf   DMA RX 缓冲区指针
 * @param  len   本次接收字节数
 *
 * 将 DMA 收到的数据拷贝到 io->rx_buffer, 并尝试提取完整命令。
 *
 * @note   调用方 (ISR) 负责在调用前后管理 DMA 通道的启停与重配置。
 *         具体见函数首尾注释代码片段。
 */
static void uart_rx_handler(const uint8_t *buf, uint32_t len)
{
    // // 读取当前接收字符数 (UART IDLE ISR 中实现)
    // uint32_t len = sizeof(rx_dma_buf) - dma_data_number_get(DMA_RX_CHANNEL);
    // dma_channel_enable(DMA_RX_CHANNEL, FALSE); // 先读状态再关DMA防止关闭后归位

    // 接口不可用则丢弃
    if (command_io_uart.available == false) 
    {
        return;
    }

    // 检测剩余空间是否足够
    uint32_t space = RX_BUFFER_LENGTH_BYTE - command_io_uart.rx_length;
    if (len > space)
    {
        // 空间不足整体丢弃
        uart_rx_buffer_flush();
        return;
    }

    // 存入缓冲区
    memcpy(&command_io_uart.rx_buffer[command_io_uart.rx_length], buf, len);
    command_io_uart.rx_length += len;

    // 检查合法命令并提取
    command_io_fetch_cmd(&command_io_uart);

    // // 重置 DMA 计数器 (UART IDLE ISR 中实现)
    // dma_data_number_set(DMA_RX_CHANNEL, sizeof(rx_dma_buf));
    // dma_channel_enable(DMA_RX_CHANNEL, TRUE);
}

/**
 * @brief  USART 接收数据处理（由 USART2 IDLE ISR 清除标志后调用）
 * @note   封装 DMA 剩余计数读取、rx_handler 拼包与 DMA 重配，
 *         中断服务函数无需感知内部缓冲与 DMA 通道。
 */
void command_io_uart_rx_isr(void)
{
    uint32_t len;

    // 先读 DMA 剩余计数再关 DMA，防止关闭后计数器归位
    len = sizeof(rx_dma_buf) - dma_data_number_get(DMA_RX_CHANNEL);
    dma_channel_enable(DMA_RX_CHANNEL, FALSE);

    command_io_uart.rx_handler(rx_dma_buf, len);

    // 重置 DMA 计数并重新使能
    dma_data_number_set(DMA_RX_CHANNEL, (uint16_t)sizeof(rx_dma_buf));
    dma_channel_enable(DMA_RX_CHANNEL, TRUE);
}

/**
 * @brief  启动一次 DMA 发送
 * @param  buf  源数据指针
 * @param  len  发送总长度
 * @retval true  发送已启动
 * @retval false 忙, 启动失败
 *
 * UART TX DMA 直接使用数据缓冲区并发起传输。
 * 
 * @note   发送完成 (idle置true) 会在发送完成中断在调用
 */
static bool uart_tx_start(const uint8_t *buf, uint32_t len)
{
    if (len == 0)               return false;
    if (len > 65535)            return false;
    if (tx_idle == false)       return false;
    if (tx_busy == true)        return false;

    // 开始发送
    tx_idle = false;
    tx_busy = true;
    uart_set_txrx_mode(true, false);

    // 配置 DMA 发送
    dma_channel_enable(DMA_TX_CHANNEL, FALSE); // 先关闭 DMA 通道，避免残留状态
    wk_dma_channel_config(DMA_TX_CHANNEL,
        (uint32_t)&IO_USART->dt,
        (uint32_t)buf,
        (uint16_t)len);
    dma_channel_enable(DMA_TX_CHANNEL, TRUE);

    // 打开 DMA_TX_CHANNEL 的完成中断
    dma_interrupt_enable(DMA_TX_CHANNEL, DMA_FDT_INT, TRUE);

    return true;
}

/**
 * @brief  TX 完成处理 (由 DMA_TX_CHANNEL TC ISR 调用)
 *
 * 等待 USART 移位寄存器清空 (TDC), 切回接收模式, 置发送空闲。
 */
static void uart_tx_handler(void)
{
    while (usart_flag_get(IO_USART, USART_TDC_FLAG) == RESET);

    // 切换接收模式
    uart_set_txrx_mode(false, true);
    tx_busy = false;
    tx_idle = true;
}

static bool uart_tx_busy(void)
{
    return tx_busy;
}

/**
 * @brief 续发下一分片 (UART 一次全发, 无需续发)
 * 
 * @return true 错误状态，正常运行过程中不可能返回true
 * @return false 忙状态
 */
static bool uart_tx_continue(void)
{
    // 单块发送忙时禁止继续发送
    if (tx_busy == true)
    {
        return false;
    }

    // 空闲状态无法继续发送
    if (tx_idle == true)
    {
        return false;
    }

    // 单块发送完成且本轮发送未结束 (非法状态)
    if (tx_busy == false && tx_idle == false)
    {
        return false;
    }

    return true; // UART 驱动单次发送全部内容，不可能进入此分支
}

static bool uart_tx_idle(void)
{
    return tx_idle;
}

command_io_t command_io_uart = 
{
    .available       = false,
    .rx_length       = 0,
    .init            = uart_init,
    .rx_buffer_flush = uart_rx_buffer_flush,
    .rx_handler      = uart_rx_handler,
    .tx_start        = uart_tx_start,
    .tx_handler      = uart_tx_handler,
    .tx_busy         = uart_tx_busy,
    .tx_continue     = uart_tx_continue,
    .tx_idle         = uart_tx_idle,
};
