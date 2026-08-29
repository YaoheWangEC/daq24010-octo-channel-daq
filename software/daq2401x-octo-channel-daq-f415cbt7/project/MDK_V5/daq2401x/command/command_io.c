#include "command_io.h"
#include "command_fifo.h"
#include <string.h>

/**
 * @brief  初始化 command_io_t 实例
 * 
 * @param  io  指向 command_io_t 实例
 */
void command_io_init(command_io_t *io)
{
    io->available = false;
    io->rx_length = 0;

    if (io->init())
    {
        io->rx_buffer_flush();
        io->available = true;
    }
}

/**
 * @brief  从接收缓冲区中提取完整命令并压入 FIFO
 * @param  io  指向 command_io 实例
 *
 * 扫描 rx_buffer 中的 \r\n 分隔符, 找到后将命令字符串压入 FIFO,
 * 并移除已消费数据。支持粘包 (一条扫描出多条命令) 和拆包 (
 * 未找到分隔符时保留数据等待下一包)。
 *
 * @note   在 ISR 上下文 (rx_handler 之后) 中调用。
 */
void command_io_fetch_cmd(command_io_t *io)
{
    while (io->rx_length >= 2)
    {
        uint32_t i;
        uint32_t limit = io->rx_length - 1;

        // 查找 \r\n
        for (i = 0; i < limit; i++)
        {
            if (io->rx_buffer[i] == '\r' && io->rx_buffer[i + 1] == '\n')
                break;
        }
        if (i == limit) return;  // 未找到

        // 提取命令, 空命令跳过
        uint32_t cmd_len = i;
        if (cmd_len > 0)
        {
            io->rx_buffer[cmd_len] = '\0';
            uint32_t len_with_nul = cmd_len + 1;

            if (len_with_nul <= CMD_MAX_LENGTH)
            {
                command_fifo_push(io, io->rx_buffer, len_with_nul);
            }
        }

        // 移除已消费部分: cmd_len 字节 + \r\n
        uint32_t consumed = i + 2;
        uint32_t remain   = io->rx_length - consumed;
        if (remain > 0)
        {
            memmove(io->rx_buffer, &io->rx_buffer[consumed], remain);
        }
        io->rx_length = remain;
    }
}
