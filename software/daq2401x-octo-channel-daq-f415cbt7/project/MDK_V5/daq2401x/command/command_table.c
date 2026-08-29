#include "command_table.h"
#include <string.h>
#include <stdio.h>

static command_t command_table[COMMAND_TABLE_MAX];
static int command_count = 0;
static char str_buf[128];

/**
 * @brief 初始化命令表
 *
 * 将命令计数器清零，准备注册新的命令。
 * 在系统启动或需要重置命令表时调用。
 */
void command_table_init(void) 
{
    command_count = 0;
}

/**
 * @brief 注册一个命令
 *
 * @param name   命令名称字符串（如 "relay"）
 * @param handler 命令处理函数指针，函数签名为 const char* func(int argc, char** argv)
 * @return 成功返回命令在表中的下标，失败返回 -1
 *
 * 用于在初始化阶段批量注册命令。
 */
int register_command(const char *name, char *(*handler)(int argc, char **argv))
{
    if (command_count >= COMMAND_TABLE_MAX) 
    {
        return -1; // 表已满
    }
    command_table[command_count].name = name;
    command_table[command_count].handler = handler;
    command_count++;
    return command_count - 1; // 成功返回下标
}

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
bool execute_command(char *input, char **output)
{
    char *argv[8];
    int argc = 0;
    bool flag = false;

    // 分词
    char *token = strtok(input, " ");
    while (token != NULL && argc < 8) 
    {
        argv[argc] = token;
        argc++;
        token = strtok(NULL, " ");
    }

    if (argc == 0) return false;

    // 查找命令
    for (int i = 0; i < command_count; i++) 
    {
        if (strcmp(argv[0], command_table[i].name) == 0) 
        {
            *output = command_table[i].handler(argc, argv);
            flag = true;
            break;
        }
    }

    if (flag == false)
    {
        snprintf(str_buf, sizeof(str_buf), "Unknown command: %s\r\n", argv[0]);
        *output = str_buf;
    }
    return flag;
}

/**
 * @brief 获取当前已注册命令数量
 *
 * @return 已注册命令的数量
 */
int command_table_count(void)
{
    return command_count;
}

/**
 * @brief 获取指定下标的命令名
 *
 * @param index 命令表下标（0 ~ count-1）
 * @return 命令名字符串，若下标非法则返回 NULL
 */
const char* command_table_get_name(int index)
{
    if (index < 0 || index >= command_count)
    {
        return NULL;
    }
    return command_table[index].name;
}
