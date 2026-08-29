#ifndef COMMAND_FIFO_H
#define COMMAND_FIFO_H

#include <stdint.h>
#include <stdbool.h>

#define CMD_FIFO_SIZE  4          /**< FIFO 容量                */
#define CMD_MAX_LENGTH 64         /**< 单条命令最大长度 (含 \0)  */

/** @brief command_io 前向声明, 避免循环依赖 */
typedef struct command_io command_io_t;

/**
 * @brief 命令 FIFO 对象
 *
 * 循环队列实现, ISR 中 push, 主循环中 pop。
 * head/tail/count 声明为 volatile, 保证跨上下文可见性。
 *
 * @note  实例定义在 command_fifo.c 中, 外部通过 API 间接访问。
 */
typedef struct
{
    char            buffer[CMD_FIFO_SIZE][CMD_MAX_LENGTH];  /**< 命令存储区       */
    uint16_t        length[CMD_FIFO_SIZE];                  /**< 每条命令长度     */
    command_io_t   *source[CMD_FIFO_SIZE];                  /**< 来源实例指针     */
    volatile uint8_t head;                                  /**< 读指针           */
    volatile uint8_t tail;                                  /**< 写指针           */
    volatile uint8_t count;                                 /**< 当前命令数量     */
} command_fifo_t;

/**
 * @brief  初始化命令 FIFO
 *
 * 将读写指针和计数器清零, 准备存储命令。
 */
void command_fifo_init(void);

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
bool command_fifo_push(command_io_t *source, const char *cmd, uint16_t len);

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
bool command_fifo_pop(command_io_t **source, char *buf, uint16_t *len);

/**
 * @brief  判断 FIFO 是否为空
 * @retval true  空
 * @retval false 非空
 */
bool command_fifo_is_empty(void);

/**
 * @brief  判断 FIFO 是否已满
 * @retval true  满
 * @retval false 未满
 */
bool command_fifo_is_full(void);

#endif
