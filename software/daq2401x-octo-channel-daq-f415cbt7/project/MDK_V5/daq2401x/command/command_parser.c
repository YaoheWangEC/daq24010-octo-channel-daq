#include "command_parser.h"
#include "command_fifo.h"
#include "command_table.h"
#include "command_handler.h"
#include "command_io.h"
#include <stdint.h>
#include <string.h>

/** @brief 解析器状态 */
typedef enum
{
    STATE_WAIT_CMD,     /**< 等待下一条命令 */
    STATE_SENDING,      /**< 正在发送响应 */
} parser_state_t;

static parser_state_t  state         = STATE_WAIT_CMD;
static command_io_t   *active_source = NULL;

/**
 * @brief  初始化命令解析器
 *
 * 注册所有支持的命令。
 */
void command_parser_init(void)
{
    state         = STATE_WAIT_CMD;
    active_source = NULL;

    command_table_init();
    register_command("lscmd",  lscmd_handler);
    register_command("echo",   echo_handler);
    register_command("device", device_handler);
    register_command("eeprom", eeprom_handler);
    register_command("reboot", reboot_handler);
    register_command("status", status_handler);
    register_command("adc",    adc_handler);
}

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
void command_parser_task(void)
{
    if (state == STATE_SENDING)
    {
        // 单块发送忙
        if (active_source->tx_busy() == true) 
        {
            return;
        }
        else
        {
            // 单块发送结束但未完成本轮发送
            if (active_source->tx_idle() == false)
            {
                if (active_source->tx_continue() == false)
                {
                    state         = STATE_WAIT_CMD;
                    active_source = NULL;
                }
                return;
            }
            else// 本轮发送结束
            {
                state         = STATE_WAIT_CMD;
                active_source = NULL;
                return;
            }
        }
    }
    else if (state == STATE_WAIT_CMD)
    {
        command_io_t *source;
        static char cmd[CMD_MAX_LENGTH];
        uint16_t len;
    
        // 尝试获取命令
        if (!command_fifo_pop(&source, cmd, &len))
        {
            return;
        }
            
        // 执行命令
        char *resp = NULL;
        execute_command(cmd, &resp);
        if (resp == NULL) return;
    
        // 开始返回响应字符串
        active_source = source;
        if (source->tx_start((const uint8_t *)resp, strlen(resp)) == true)
        {
            state = STATE_SENDING;
        }
        return;
    }
}
