#ifndef COMMAND_TABLE_H
#define COMMAND_TABLE_H

#include <stdint.h>
#include <stdbool.h>

// 命令表最大长度
#define COMMAND_TABLE_MAX 16

/** 命令结构体 */
typedef struct 
{
    const char *name;                           /**< 命令名 */
    char *(*handler)(int argc, char **argv);    /**< 命令处理函数，返回字符串 */
} command_t;

/**
 * @brief 初始化命令表
 *
 * 将命令计数器清零，准备注册新的命令。
 * 在系统启动或需要重置命令表时调用。
 */
void command_table_init(void);

/**
 * @brief 注册一个命令
 *
 * @param name   命令名称字符串（如 "relay"）
 * @param handler 命令处理函数指针，函数签名为 const char* func(int argc, char** argv)
 * @return 成功返回命令在表中的下标，失败返回 -1
 *
 * 用于在初始化阶段批量注册命令。
 */
int register_command(const char *name, char *(*handler)(int argc, char **argv));

/**
 * @brief 执行命令
 *
 * @param input   输入的命令字符串（以空格分隔参数）
 * @param output  指向字符串指针的指针，用于返回命令执行结果
 * @return true 表示找到并执行了命令，false 表示未找到命令
 *
 * 该函数会解析输入字符串，查找对应命令并调用处理函数。
 * 如果命令不存在，会返回错误提示字符串。
 */
bool execute_command(char *input, char **output);

/**
 * @brief 获取当前已注册命令数量
 *
 * @return 已注册命令的数量
 */
int command_table_count(void);

/**
 * @brief 获取指定下标的命令名
 *
 * @param index 命令表下标（0 ~ count-1）
 * @return 命令名字符串，若下标非法则返回 NULL
 */
const char* command_table_get_name(int index);


#endif // COMMAND_TABLE_H
