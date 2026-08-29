#include "command_fifo.h"
#include <string.h>

/** @brief 命令 FIFO 全局单例 */
static command_fifo_t cmd_fifo;

/**
 * @brief  初始化命令 FIFO
 *
 * 将读写指针和计数器清零, 准备存储命令。
 */
void command_fifo_init(void)
{
    cmd_fifo.head  = 0;
    cmd_fifo.tail  = 0;
    cmd_fifo.count = 0;
}

/**
 * @brief  将命令压入 FIFO
 * @param  source  命令来源实例指针
 * @param  cmd     命令字符串
 * @param  len     命令长度 (含 \0)
 * @retval true    压入成功
 * @retval false   FIFO 已满或命令超长
 *
 * @note   注意若与pop不在同一优先级会在count计数时存在竞争
 */
bool command_fifo_push(command_io_t *source, const char *cmd, uint16_t len)
{
    if (cmd_fifo.count >= CMD_FIFO_SIZE) return false;
    if (len > CMD_MAX_LENGTH)          return false;

    memcpy(cmd_fifo.buffer[cmd_fifo.tail], cmd, len);
    cmd_fifo.length[cmd_fifo.tail] = len;
    cmd_fifo.source[cmd_fifo.tail] = source;

    cmd_fifo.tail = (cmd_fifo.tail + 1) % CMD_FIFO_SIZE;
    cmd_fifo.count++;
    return true;
}

/**
 * @brief  从 FIFO 中弹出一条命令
 * @param  source  输出: 命令来源实例指针
 * @param  buf     输出缓冲区 (存放命令字符串)
 * @param  len     输出: 命令长度 (含 \0)
 * @retval true    弹出成功
 * @retval false   FIFO 为空
 *
 * @note   注意若与push不在同一优先级会在count计数时存在竞争
 */
bool command_fifo_pop(command_io_t **source, char *buf, uint16_t *len)
{
    if (cmd_fifo.count == 0) return false;

    memcpy(buf, cmd_fifo.buffer[cmd_fifo.head], cmd_fifo.length[cmd_fifo.head]);
    *len    = cmd_fifo.length[cmd_fifo.head];
    *source = cmd_fifo.source[cmd_fifo.head];

    cmd_fifo.head = (cmd_fifo.head + 1) % CMD_FIFO_SIZE;
    cmd_fifo.count--;
    return true;
}

/**
 * @brief  判断 FIFO 是否为空
 * @retval true  空
 * @retval false 非空
 */
bool command_fifo_is_empty(void)
{
    return cmd_fifo.count == 0;
}

/**
 * @brief  判断 FIFO 是否已满
 * @retval true  满
 * @retval false 未满
 */
bool command_fifo_is_full(void)
{
    return cmd_fifo.count >= CMD_FIFO_SIZE;
}
