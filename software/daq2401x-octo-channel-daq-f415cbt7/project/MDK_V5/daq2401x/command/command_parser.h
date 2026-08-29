#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

/**
 * @brief  初始化命令解析器
 *
 * 注册所有支持的命令。
 */
void command_parser_init(void);

/**
 * @brief  命令解析任务 (主循环每次迭代调用)
 *
 * 状态机:
 *   WAIT_CMD  → pop FIFO, 执行命令, tx_start, 进入 SENDING
 *   SENDING   → 检查分片发送进度, 续发或回到 WAIT_CMD
 *
 * @note   阻塞仅发生在 tx_busy() 返回 true 时等待下一轮主循环,
 *         不忙等硬件。
 */
void command_parser_task(void);

#endif
