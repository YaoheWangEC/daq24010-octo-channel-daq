#include "command_handler.h"
#include "command_table.h"
#include "global_vars.h"
#include "bsp.h"
#include "at32f415_wk_config.h"
#include "wk_system.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char resp[1024];


char* lscmd_handler(int argc, char **argv)
{
    // 帮助信息
    if (argc == 2 && 
       (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        sprintf(resp,
            "Usage: lscmd [options]\r\n"
            "Options:\r\n"
            "  -h, --help    Show this help message\r\n"
            "\r\n"
            "List all supported commands.\r\n");
        return resp;
    }
    
    if (argc == 1)
    {
        // 默认行为：列出所有命令
        int count = command_table_count();
        int offset = snprintf(resp, sizeof(resp), "Supported commands:\r\n");
        for (int i = 0; i < count && offset < sizeof(resp); i++)
        {
            const char* name = command_table_get_name(i);
            if (name)
            {
                offset += snprintf(resp + offset, sizeof(resp) - offset, "  %s\r\n", name);
            }
        }
        return resp;
    }

    // 无效参数
    sprintf(resp, "Invalid option. Try 'lscmd -h'\r\n");
    return resp;
}

char* echo_handler(int argc, char **argv)
{
    // 帮助信息
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage: echo [text]\r\n"
                 "Options:\r\n"
                 "  -h, --help    Show this help message\r\n"
                 "\r\n"
                 "Print the given text to output.\r\n");
        return resp;
    }

    // 默认行为：拼接参数并返回
    if (argc > 1)
    {
        resp[0] = '\0';
        for (int i = 1; i < argc; i++)
        {
            strncat(resp, argv[i], sizeof(resp) - strlen(resp) - 1);
            if (i < argc - 1)
                strncat(resp, " ", sizeof(resp) - strlen(resp) - 1);
        }
        strncat(resp, "\r\n", sizeof(resp) - strlen(resp) - 1);
        return resp;
    }

    snprintf(resp, sizeof(resp), "\r\n");
    return resp;
}

char* device_handler(int argc, char **argv)
{
    if (argc == 2 &&
       (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage: device\r\n"
                 "\r\n"
                 "Show device model, name and serial number.\r\n");
        return resp;
    }

    snprintf(resp, sizeof(resp),
             "Name  : DAQ24010 octo-channel-universal-daq\r\n"
             "UID   : %08X%08X%08X\r\n",
             (unsigned)g_vars.uid[0],
             (unsigned)g_vars.uid[1],
             (unsigned)g_vars.uid[2]);
    return resp;
}

char* reboot_handler(int argc, char **argv)
{
    if (argc == 2 &&
       (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage: reboot\r\n"
                 "\r\n"
                 "Force MCU reset by pulling NRST low.\r\n");
        return resp;
    }

    // 拉低 NRST_CON(PB9) 触发强制复位
    gpio_bits_reset(NRST_CON_GPIO_PORT, NRST_CON_PIN);

    // 忙等 10ms 覆盖复位脉冲；复位成功则 MCU 已重启，执行不到后续代码
    wk_delay_ms(10);

    // 执行到这里即复位失败
    snprintf(resp, sizeof(resp), "ERR: reboot fail\r\n");
    return resp;
}

// 单字节十六进制字符 → 数值；非法字符返回 -1
static int hex_char_to_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char* eeprom_handler(int argc, char **argv)
{
    // 帮助信息
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage:\r\n"
                 "  eeprom read <addr> [len]\r\n"
                 "  eeprom write <addr> <hex>\r\n"
                 "\r\n"
                 "Read/write AT24C02 (256 bytes).\r\n"
                 "  addr: 0~255\r\n"
                 "  read : len 1~16 bytes (default 1), range must stay within 256\r\n"
                 "  write: hex = even-length hex string, up to 4 bytes\r\n");
        return resp;
    }

    if (argc < 3)
    {
        snprintf(resp, sizeof(resp), "Invalid args. Try 'eeprom -h'\r\n");
        return resp;
    }

    if (strcmp(argv[1], "read") == 0)
    {
        char *end = NULL;
        long addr = strtol(argv[2], &end, 0);
        // 校验 addr 参数：必须是完整数字
        if (end == argv[2] || *end != '\0')
        {
            snprintf(resp, sizeof(resp), "ERR: invalid addr\r\n");
            return resp;
        }
        long len = 1;
        if (argc >= 4)
        {
            end = NULL;
            len = strtol(argv[3], &end, 0);
            if (end == argv[3] || *end != '\0')
            {
                snprintf(resp, sizeof(resp), "ERR: invalid len\r\n");
                return resp;
            }
        }

        // 校验：addr 0~255，len 1~16，且 addr+len 不越界（越界则直接报错，不返回任何数据）
        if ((addr < 0) || (addr > 255) || (len < 1) || (len > 16) || (addr + len > 256))
        {
            snprintf(resp, sizeof(resp), "ERR: out of range\r\n");
            return resp;
        }

        uint8_t buf[16];
        if (!at24c02_read((uint16_t)addr, buf, (uint16_t)len))
        {
            snprintf(resp, sizeof(resp), "ERR: read fail\r\n");
            return resp;
        }

        // 输出紧凑十六进制（无分隔，与写入格式对称）
        int off = 0;
        for (long i = 0; i < len && off < (int)sizeof(resp) - 4; i++)
        {
            off += snprintf(resp + off, sizeof(resp) - (size_t)off, "%02X",
                            (unsigned)buf[i]);
        }
        snprintf(resp + off, sizeof(resp) - (size_t)off, "\r\n");
        return resp;
    }

    if (strcmp(argv[1], "write") == 0)
    {
        if (argc < 4)
        {
            snprintf(resp, sizeof(resp), "Usage: eeprom write <addr> <hex>\r\n");
            return resp;
        }

        char *end = NULL;
        long addr = strtol(argv[2], &end, 0);
        // 校验 addr 参数：必须是完整数字
        if (end == argv[2] || *end != '\0')
        {
            snprintf(resp, sizeof(resp), "ERR: invalid addr\r\n");
            return resp;
        }
        const char *hex = argv[3];
        size_t hex_len = strlen(hex);

        // 上限 4 字节（8 个 hex 字符），偶数长度
        if ((addr < 0) || (addr > 255) || (hex_len == 0) || (hex_len > 8) || (hex_len % 2) != 0)
        {
            snprintf(resp, sizeof(resp), "ERR: invalid addr or hex\r\n");
            return resp;
        }
        if ((long)(addr + hex_len / 2) > 256)
        {
            snprintf(resp, sizeof(resp), "ERR: out of range\r\n");
            return resp;
        }

        uint8_t buf[4];
        for (size_t i = 0; i < hex_len / 2; i++)
        {
            int hi = hex_char_to_val(hex[i * 2]);
            int lo = hex_char_to_val(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
            {
                snprintf(resp, sizeof(resp), "ERR: invalid hex\r\n");
                return resp;
            }
            buf[i] = (uint8_t)((hi << 4) | lo);
        }

        if (!at24c02_write((uint16_t)addr, buf, (uint16_t)(hex_len / 2)))
        {
            snprintf(resp, sizeof(resp), "ERR: write fail\r\n");
            return resp;
        }

        snprintf(resp, sizeof(resp), "OK\r\n");
        return resp;
    }

    snprintf(resp, sizeof(resp), "Invalid args. Try 'eeprom -h'\r\n");
    return resp;
}

// 触发模式枚举 → 字符串
static const char* trigger_mode_str(adc_trigger_mode_t mode)
{
    switch (mode)
    {
    case ADC_TRIGGER_MODE_SOFTWARE:      return "software";
    case ADC_TRIGGER_MODE_LEVEL_RISING:  return "level+";
    case ADC_TRIGGER_MODE_LEVEL_FALLING: return "level-";
    case ADC_TRIGGER_MODE_SLOPE_RISING:  return "slope+";
    case ADC_TRIGGER_MODE_SLOPE_FALLING: return "slope-";
    default:                             return "error";
    }
}

// 触发模式字符串 → 枚举；非法返回 -1
static int trigger_mode_val(const char *s)
{
    if (strcmp(s, "software") == 0) return ADC_TRIGGER_MODE_SOFTWARE;
    if (strcmp(s, "level+") == 0)   return ADC_TRIGGER_MODE_LEVEL_RISING;
    if (strcmp(s, "level-") == 0)   return ADC_TRIGGER_MODE_LEVEL_FALLING;
    if (strcmp(s, "slope+") == 0)   return ADC_TRIGGER_MODE_SLOPE_RISING;
    if (strcmp(s, "slope-") == 0)   return ADC_TRIGGER_MODE_SLOPE_FALLING;
    return -1;
}

// 采集状态枚举 → 字符串
static const char* acq_state_str(adc_acq_state_t st)
{
    switch (st)
    {
    case ADC_STATE_IDLE:         return "idle";
    case ADC_STATE_WAIT_TRIGGER: return "wait";
    case ADC_STATE_ACQUIRING:    return "acq";
    default:                     return "error";
    }
}

char* status_handler(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage:\r\n"
                 "  status                     show all\r\n"
                 "  status <field>            show one field\r\n"
                 "  status channel <1..8>     set channel (IDLE only)\r\n"
                 "  status rate <rate>        set decimated rate (IDLE only)\r\n"
                 "  status trigger <mode> [thr]  set trigger (IDLE only)\r\n"
                 "\r\n"
                 "Fields: tick channel raw_rate rate trigger state avdd\r\n"
                 "Trigger mode: software level+ level- slope+ slope-\r\n"
                 "Rate (Hz): 1000 2000 3000 4000 5000 6000 8000 10000\r\n"
                 "           20000 30000 40000 50000 60000 100000 120000 200000\r\n");
        return resp;
    }

    // status：无参数 = 全量展示
    if (argc == 1)
    {
        snprintf(resp, sizeof(resp),
                 "tick     : %lu\r\n"
                 "channel  : %u\r\n"
                 "raw_rate : %lu\r\n"
                 "dec_rate : %lu\r\n"
                 "trigger  : %s @ %d\r\n"
                 "state    : %s\r\n"
                 "avdd     : %.3f\r\n",
                 (unsigned long)g_vars.tick,
                 (unsigned)adc_logic_channel_get(),
                 (unsigned long)g_vars.raw_samplerate,
                 (unsigned long)g_vars.decimated_samplerate,
                 trigger_mode_str(g_vars.trigger_mode),
                 (int)g_vars.trigger_threshold,
                 acq_state_str(g_vars.acq_state),
                 (double)g_vars.avdd);
        return resp;
    }

    // status <field> [value]：field 至少需要
    if (argc < 2)
    {
        snprintf(resp, sizeof(resp), "Invalid args. Try 'status -h'\r\n");
        return resp;
    }

    const char *field = argv[1];

    // ---------- 展示 ----------
    if (argc == 2)
    {
        if (strcmp(field, "tick") == 0)
        {
            snprintf(resp, sizeof(resp), "%lu\r\n", (unsigned long)g_vars.tick);
            return resp;
        }
        if (strcmp(field, "channel") == 0)
        {
            snprintf(resp, sizeof(resp), "%u\r\n", (unsigned)adc_logic_channel_get());
            return resp;
        }
        if (strcmp(field, "raw_rate") == 0)
        {
            snprintf(resp, sizeof(resp), "%lu\r\n", (unsigned long)g_vars.raw_samplerate);
            return resp;
        }
        if (strcmp(field, "rate") == 0)
        {
            snprintf(resp, sizeof(resp), "%lu\r\n", (unsigned long)g_vars.decimated_samplerate);
            return resp;
        }
        if (strcmp(field, "trigger") == 0)
        {
            snprintf(resp, sizeof(resp), "%s @ %d\r\n",
                     trigger_mode_str(g_vars.trigger_mode),
                     (int)g_vars.trigger_threshold);
            return resp;
        }
        if (strcmp(field, "state") == 0)
        {
            snprintf(resp, sizeof(resp), "%s\r\n", acq_state_str(g_vars.acq_state));
            return resp;
        }
        if (strcmp(field, "avdd") == 0)
        {
            snprintf(resp, sizeof(resp), "%.3f\r\n", (double)g_vars.avdd);
            return resp;
        }

        snprintf(resp, sizeof(resp), "ERR: unknown field '%s'\r\n", field);
        return resp;
    }

    // ---------- 修改（仅 IDLE） ----------
    if (g_vars.acq_state != ADC_STATE_IDLE)
    {
        snprintf(resp, sizeof(resp), "ERR: busy\r\n");
        return resp;
    }

    if (strcmp(field, "channel") == 0)
    {
        char *end = NULL;
        long n = strtol(argv[2], &end, 0);
        if (end == argv[2] || *end != '\0')
        {
            snprintf(resp, sizeof(resp), "ERR: invalid channel\r\n");
            return resp;
        }
        if (n < 1 || n > 8)
        {
            snprintf(resp, sizeof(resp), "ERR: channel must be 1~8\r\n");
            return resp;
        }
        adc_logic_channel_set_by_num((uint8_t)n);
        snprintf(resp, sizeof(resp), "channel  : %u\r\n", (unsigned)adc_logic_channel_get());
        return resp;
    }

    if (strcmp(field, "rate") == 0)
    {
        char *end = NULL;
        long r = strtol(argv[2], &end, 0);
        if (end == argv[2] || *end != '\0')
        {
            snprintf(resp, sizeof(resp), "ERR: invalid rate\r\n");
            return resp;
        }

        // 枚举表内查找
        static const long rate_table[] =
        {
            1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000,
            20000, 30000, 40000, 50000, 60000, 100000, 120000, 200000
        };
        bool found = false;
        adc_decimated_samplerate_t rate = ADC_DECIMATED_SAMPLERATE_1KSPS;
        for (size_t i = 0; i < sizeof(rate_table) / sizeof(rate_table[0]); i++)
        {
            if (r == rate_table[i])
            {
                rate = (adc_decimated_samplerate_t)r;
                found = true;
                break;
            }
        }
        if (!found)
        {
            snprintf(resp, sizeof(resp), "ERR: unsupported rate\r\n");
            return resp;
        }

        if (!adc_decimated_samplerate_set(rate))
        {
            snprintf(resp, sizeof(resp), "ERR: set rate fail\r\n");
            return resp;
        }
        snprintf(resp, sizeof(resp), "raw_rate : %lu\r\ndec_rate : %lu\r\n",
                 (unsigned long)g_vars.raw_samplerate,
                 (unsigned long)g_vars.decimated_samplerate);
        return resp;
    }

    if (strcmp(field, "trigger") == 0)
    {
        int mode = trigger_mode_val(argv[2]);
        if (mode < 0)
        {
            snprintf(resp, sizeof(resp), "ERR: invalid trigger mode\r\n");
            return resp;
        }

        long thr = 0;
        if (argc >= 4)
        {
            char *end = NULL;
            thr = strtol(argv[3], &end, 0);
            if (end == argv[3] || *end != '\0')
            {
                snprintf(resp, sizeof(resp), "ERR: invalid threshold\r\n");
                return resp;
            }
        }

        if (!adc_trigger_set((adc_trigger_mode_t)mode, (int16_t)thr))
        {
            snprintf(resp, sizeof(resp), "ERR: set trigger fail\r\n");
            return resp;
        }
        snprintf(resp, sizeof(resp), "trigger  : %s @ %d\r\n",
                 trigger_mode_str(g_vars.trigger_mode),
                 (int)g_vars.trigger_threshold);
        return resp;
    }

    snprintf(resp, sizeof(resp), "ERR: unknown field '%s'\r\n", field);
    return resp;
}

char* adc_handler(int argc, char **argv)
{
    // 帮助信息
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        snprintf(resp, sizeof(resp),
                 "Usage:\r\n"
                 "  adc                show acquisition config\r\n"
                 "  adc status         show acq state only\r\n"
                 "  adc start          start acquisition\r\n"
                 "  adc stop           force stop acquisition\r\n"
                 "  adc rest           show remaining sample count\r\n"
                 "  adc read [n]       read up to n samples as compact hex (default/max 128)\r\n"
                 "\r\n"
                 "read: 4 hex chars per sample (12bit in 16bit), no separator.\r\n"
                 "      Returns available data even if fewer than n.\r\n");
        return resp;
    }

    // adc：无参数 = 配置全量展示
    if (argc == 1)
    {
        snprintf(resp, sizeof(resp),
                 "state    : %s\r\n"
                 "channel  : %u\r\n"
                 "raw_rate : %lu\r\n"
                 "dec_rate : %lu\r\n"
                 "trigger  : %s @ %d\r\n"
                 "avdd     : %.3f\r\n"
                 "buf_cnt  : %u\r\n"
                 "buf_cap  : %u\r\n"
                 "buf_full : %s\r\n"
                 "overflow : %lu\r\n",
                 acq_state_str(g_vars.acq_state),
                 (unsigned)adc_logic_channel_get(),
                 (unsigned long)g_vars.raw_samplerate,
                 (unsigned long)g_vars.decimated_samplerate,
                 trigger_mode_str(g_vars.trigger_mode),
                 (int)g_vars.trigger_threshold,
                 (double)g_vars.avdd,
                 (unsigned)ringbuf_len(&g_vars.adc_ring),
                 (unsigned)g_vars.adc_ring.len,
                 ringbuf_is_full(&g_vars.adc_ring) ? "yes" : "no",
                 (unsigned long)ringbuf_overflow_cnt(&g_vars.adc_ring));
        return resp;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        // 仅采集状态，便于自动化读取
        snprintf(resp, sizeof(resp), "%s\r\n", acq_state_str(g_vars.acq_state));
        return resp;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        adc_start();   // 内部已处理强制停止（若运行中）
        snprintf(resp, sizeof(resp), "OK\r\n");
        return resp;
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        adc_stop();
        snprintf(resp, sizeof(resp), "OK\r\n");
        return resp;
    }

    if (strcmp(argv[1], "rest") == 0)
    {
        // 仅返回剩余样本数，便于自动化轮询
        snprintf(resp, sizeof(resp), "%u\r\n", (unsigned)ringbuf_len(&g_vars.adc_ring));
        return resp;
    }

    if (strcmp(argv[1], "read") == 0)
    {
        long want = 128;   // 默认最长
        if (argc >= 3)
        {
            char *end = NULL;
            want = strtol(argv[2], &end, 0);
            if (end == argv[2] || *end != '\0')
            {
                snprintf(resp, sizeof(resp), "ERR: invalid count\r\n");
                return resp;
            }
            if (want < 1 || want > 128)
            {
                snprintf(resp, sizeof(resp), "ERR: count must be 1~128\r\n");
                return resp;
            }
        }

        // 逐样本弹出，4 hex 字符紧凑输出；不足则发出已有数据
        // 先按响应缓冲容量收敛上限，保证弹出的样本一定被写出（每样本 4 字符）
        long max_n = (long)((sizeof(resp) - 4) / 4);
        if (want > max_n)
        {
            want = max_n;
        }

        int off = 0;
        uint16_t sample;
        while ((want-- > 0) && ringbuf_pop(&g_vars.adc_ring, &sample))
        {
            off += snprintf(resp + off, sizeof(resp) - (size_t)off, "%04X",
                            (unsigned)sample);
        }
        snprintf(resp + off, sizeof(resp) - (size_t)off, "\r\n");
        return resp;
    }

    snprintf(resp, sizeof(resp), "Invalid args. Try 'adc -h'\r\n");
    return resp;
}


