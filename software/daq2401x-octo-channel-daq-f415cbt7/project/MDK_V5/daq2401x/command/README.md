# Linux CLI 风味 Command 驱动框架 Linux CLI-style Command Driver Framework

裸机平台下的多接口命令驱动框架, 支持 USB CDC / UART (RS485/RS232/TTL) 等接口, 统一以中断接收、DMA 发送的方式工作。

---

## 1. 概述

### 设计原则

- **ISR 最小化** — 中断中只做数据搬运与拼包, 解析、发送状态机均由主循环驱动；
- **一问一答** — 命令串行处理, 当前回复全部发送完毕后才会处理下一条命令；
- **多接口路由** — 命令携带来源实例指针, 回复自动从原路返回；
- **最小化阻塞** — TX 分片发送, 主循环推进, 不忙等硬件。

### 文件清单

| 文件 | 职责 |
|---|---|
| `command_io.h` / `.c` | 抽象 IO 实例定义、拼包提取 |
| `command_fifo.h` / `.c` | 全局命令 FIFO (ISR push, 主循环 pop) |
| `command_parser.h` / `.c` | 命令解析状态机 |
| `command_table.h` / `.c` | 命令注册表与派发 |
| `command_handler.h` / `.c` | 内置命令实现 |
| `command_io_usb.c` | USB CDC 驱动 |
| `command_io_uart.c` | UART 驱动 (DMA + IDLE) |

---

## 2. 架构

### 分层示意

```
┌──────────────────────────────────┐
│  command_handler                 │  ← 命令实现 (echo, device, ...)
├──────────────────────────────────┤
│  command_table                   │  ← 注册 / 查表 / 执行
├──────────────────────────────────┤
│  command_parser (状态机)         │  ← FIFO pop → 派发 → TX 管理
├──────────────┬───────────────────┤
│ command_fifo │  command_io       │  ← 共享 FIFO + IO 实例抽象
│ (全局单例)   │  (rx拼包/tx分片)  │
├──────────────┴───────────────────┤
│  command_io_usb  │ command_io_uart│  ← 硬件驱动层
└──────────────────────────────────┘
```

### RX 数据流

```
Host 发送命令
  → 硬件 ISR (USB BULK OUT / UART IDLE)
    → io->rx_handler(buf, len)  追加到 rx_buffer
    → command_io_fetch_cmd()    扫描 \r\n → 推入 command_fifo
  → 主循环 command_parser_task()
    → command_fifo_pop()        取出命令
    → execute_command()         派发 handler
    → io->tx_start()            启动发送
```

### TX 数据流

```
主循环
  → tx_start()           发送首分片
  → ISR: tx_handler()    置发送完成标志
  → 主循环: tx_busy?     等待
  → tx_continue()        续发下一分片 / ZLP
  → tx_idle()            全部完成
```

---

## 3. 快速集成

### 3.1 启动初始化

```c
void setup(void)
{
    command_fifo_init();                     // 初始化全局 FIFO
    command_io_init(&command_io_usb);        // 初始化 IO 实例
    command_parser_init();                   // 注册命令 (lscmd, echo, device)
}
```

### 3.2 主循环

```c
void loop(void)
{
    other_task();
    command_parser_task();                   // 命令处理
}
```

---

## 4. 接口驱动开发

### 4.1 `command_io_t` 接口

每个 IO 实例需实现以下 8 个函数:

| 函数指针 | 调用上下文 | 职责 |
|---|---|---|
| `init()` | 主循环启动 | 初始化硬件, 返回 `true` 成功 |
| `rx_buffer_flush()` | 主循环 / ISR | 清空 `rx_length` |
| `rx_handler(buf, len)` | ISR | 将 `buf` 数据追加到 `io->rx_buffer`, 调用 `command_io_fetch_cmd` |
| `tx_start(buf, len)` | 主循环 | 启动一次分片发送, 记录 `buf` 与总长度 |
| `tx_handler()` | ISR | 置发送完成标志 |
| `tx_busy()` | 主循环 | 查询硬件是否忙 |
| `tx_continue()` | 主循环 | 从记录位置续发下一分片 |
| `tx_idle()` | 主循环 | 查询本轮发送是否全部完成 |

### 4.2 USB CDC 驱动

文件: `command_io_usb.c`

- 基于 CherryUSB 协议栈，使用 USB FS 的 CDC 虚拟串口。
- CherryUSB 回调 (`usbd_cdc_acm_bulk_out/in_callback`) 桥接到驱动函数
- TX 按 CDC_MAX_MPS (64B) 分片, 自动处理 ZLP
- `tx_start(buf, len)` 记录 `buf` 与 `len`, 发首分片; `tx_continue` 续发

### 4.3 UART 驱动

文件: `command_io_uart.c`

- 基于 DMA normal 模式 + IDLE 中断
- TX 一次 DMA 全发, 不分片 (`tx_continue` 始终返回 `false`)
- `tx_handler` 等待 USART TDC 标志后切回接收模式 (RS485)
- 头文件内置总线适配说明 (`@section bus_mode`)

### 4.4 自定义驱动

1. 创建 `command_io_xxx.c`, include `command_io.h`
2. 实现 8 个函数 (参考 `command_io_usb.c`)
3. 定义 `command_io_t command_io_xxx` 实例
4. 在 `command_io.h` 中添加 `extern command_io_t command_io_xxx;`
5. 在应用初始化中调用 `command_io_init(&command_io_xxx)`

---

## 5. 命令开发

### 5.1 注册

```c
// command_parser.c → command_parser_init()
register_command("mycmd", mycmd_handler);
```

### 5.2 Handler 规范

```c
/**
 * @brief  命令处理函数
 * @param  argc  参数个数
 * @param  argv  参数数组
 * @return 响应字符串 (指向静态缓冲区, parser 阻塞消费)
 */
char* mycmd_handler(int argc, char **argv)
{
    static char resp[256]; // 推荐使用全局的resp以节省RAM

    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        snprintf(resp, sizeof(resp), "Usage: mycmd [args]\r\n");
        return resp;
    }

    snprintf(resp, sizeof(resp), "OK\r\n");
    return resp;
}
```

- 返回指针必须指向静态或全局缓冲区, TX 完成前不得覆盖
- 响应必须以 `\r\n` 结尾 (Host 终端换行)
- 返回值 `NULL` 表示无响应

### 5.3 内置命令

| 命令 | 说明 |
|---|---|
| `lscmd` | 列出所有已注册命令 |
| `echo <text>` | 回显参数 |
| `device` | 显示设备型号、名称、UID |

---

## 6. API 参考

### 6.1 command_io.h

```c
#define RX_BUFFER_LENGTH_BYTE (64 * 2)  // RX 命令拼包缓冲区大小

extern command_io_t command_io_usb;      // USB CDC 实例
extern command_io_t command_io_uart;     // UART 实例

void command_io_init(command_io_t *io);               // 初始化实例
void command_io_fetch_cmd(command_io_t *io);          // 从 rx_buffer 提取命令
```

### 6.2 command_fifo.h

```c
#define CMD_FIFO_SIZE  4                  // FIFO 容量
#define CMD_MAX_LENGTH 64                 // 单条命令最大长度

void command_fifo_init(void);
bool command_fifo_push(command_io_t *source, const char *cmd, uint16_t len);
bool command_fifo_pop(command_io_t **source, char *buf, uint16_t *len);
bool command_fifo_is_empty(void);
bool command_fifo_is_full(void);
```

### 6.3 command_parser.h

```c
void command_parser_init(void);           // 注册所有命令
void command_parser_task(void);           // 主循环状态机
```

### 6.4 command_table.h

```c
#define COMMAND_TABLE_MAX 16

void command_table_init(void);
int  register_command(const char *name, char *(*handler)(int argc, char **argv));
bool execute_command(char *input, char **output);
int  command_table_count(void);
const char* command_table_get_name(int index);
```

---

## 7. 移植指南

### 7.1 宏定义

| 宏 | 默认值 | 说明 |
|---|---|---|
| `CMD_FIFO_SIZE` | 4 | FIFO 容量, 按需增大 |
| `CMD_MAX_LENGTH` | 64 | 单条命令最大长度 |
| `RX_BUFFER_LENGTH_BYTE` | 128 | 跨包拼接空间, 应 ≥ 2 倍最长包 |
| `COMMAND_TABLE_MAX` | 16 | 最大注册命令数 |

### 7.2 ISR 注册 (UART)

```c
// 在项目 ISR 文件中注册:
void USART2_IRQHandler(void)
{
    if (usart_flag_get(USART2, USART_IDLEF_FLAG) != RESET)
    {
        usart_flag_clear(USART2, USART_IDLEF_FLAG);

        uint32_t len = sizeof(rx_dma_buf) - dma_data_number_get(DMA_RX_CHANNEL);
        dma_channel_enable(DMA_RX_CHANNEL, FALSE);

        command_io_uart.rx_handler(rx_dma_buf, len);

        dma_data_number_set(DMA_RX_CHANNEL, sizeof(rx_dma_buf));
        dma_channel_enable(DMA_RX_CHANNEL, TRUE);
    }
}

void DMA1_Channel4_IRQHandler(void)       // UART TX DMA
{
    if (dma_flag_get(DMA1_FDT4_FLAG) != RESET)
    {
        dma_flag_clear(DMA1_FDT4_FLAG);
        command_io_uart.tx_handler();
    }
}
```

### 7.3 UART类总线适配

修改 `command_io_uart.c` 中的 `uart_set_txrx_mode()`:

| 总线 | TX 操作 | RX 操作 |
|---|---|---|
| RS485 | DE 拉高 | DE 拉低 |
| RS422 | TX 使能 | RX 使能 |
| RS232/TTL | 收发可同时使能 | 全双工, 删除互斥检查 |
