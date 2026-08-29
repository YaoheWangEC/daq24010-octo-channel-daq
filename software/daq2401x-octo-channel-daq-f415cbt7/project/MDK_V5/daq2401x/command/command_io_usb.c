#include "command_io.h"
#include "usb_cdc_uac.h"
#include <string.h>

static volatile bool tx_idle;                          /* 一轮发送是否空闲 */
static volatile bool tx_busy;                          /* 单块发送是否忙   */

static const uint8_t * volatile tx_buf;                /* 发送缓冲区       */
static volatile uint32_t tx_total_len;                 /* 发送总长度       */
static volatile uint32_t tx_rest;                      /* 发送剩余长度     */
static volatile bool require_send_zlp;                 /* 需要发送零长度包 */

static bool usb_init(void)
{
    tx_idle = true;
    tx_busy = false;
    return true;
}

static void usb_rx_buffer_flush(void)
{
    command_io_usb.rx_length = 0;
}

static void usb_rx_handler(const uint8_t *buf, uint32_t len)
{
    // 接口不可用则丢弃
    if (command_io_usb.available == false)
    {
        return;
    }

    // 检测剩余空间是否足够
    uint32_t space = RX_BUFFER_LENGTH_BYTE - command_io_usb.rx_length;
    if (len > space)
    {
        // 空间不足整体丢弃
        usb_rx_buffer_flush();
        return;
    }

    // 存入缓冲区
    memcpy(&command_io_usb.rx_buffer[command_io_usb.rx_length], buf, len);
    command_io_usb.rx_length += len;

    // 检查合法命令并提取
    command_io_fetch_cmd(&command_io_usb);
}

static bool usb_tx_start(const uint8_t *buf, uint32_t len)
{
    // 检测是否空闲
    if ((tx_idle == false) || (tx_busy == true))
    {
        return false;
    }

    // 开始发送
    tx_idle = false;
    tx_buf = buf;
    tx_total_len = len;
    tx_rest = tx_total_len;
    require_send_zlp = false;

    // 计算包长
    uint8_t chunk_len = (tx_rest > 64) ? 64 : tx_rest;
    if (((tx_rest - chunk_len) == 0) && (chunk_len == 64))
    {
        require_send_zlp = true;
    }

    // 开始发送
    tx_busy = true;
    if (usb_cdc_send(&tx_buf[tx_total_len - tx_rest], chunk_len) == true)
    {
        tx_rest -= chunk_len;
    }
    else
    {
        tx_idle = true;
        tx_busy = false;
        return false;
    }

    return true;
}

static void usb_tx_handler(void)
{
    tx_busy = false;
    
    // 所有内容发送完成且无需发送ZLP
    if ((tx_rest == 0) && (require_send_zlp == false))
    {
        tx_idle = true;
    }
}

static bool usb_tx_busy(void)
{
    return tx_busy;
}

static bool usb_tx_continue(void)
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

    // 发送ZLP
    if (tx_rest == 0 && require_send_zlp == true)
    {
        tx_busy = true;

        if (usb_cdc_send(NULL, 0))
        {
            require_send_zlp = false;
        }
        else
        {
            tx_busy = false;
        }
        return true;
    }

    // 发送剩余内容
    uint32_t chunk_len = (tx_rest > 64) ? 64 : tx_rest;
    if ((tx_rest - chunk_len) == 0 && chunk_len == 64)
    {
        require_send_zlp = true;
    }

    if (usb_cdc_send(&tx_buf[tx_total_len - tx_rest], chunk_len) == true)
    {
        tx_rest -= chunk_len;
    }
    else
    {
        tx_idle = true;
        tx_busy = false;
        return false;
    }

    return true;
}

static bool usb_tx_idle(void)
{
    return tx_idle;
}

command_io_t command_io_usb = 
{
    .available       = false,
    .rx_length       = 0,
    .init            = usb_init,
    .rx_buffer_flush = usb_rx_buffer_flush,
    .rx_handler      = usb_rx_handler,
    .tx_start        = usb_tx_start,
    .tx_handler      = usb_tx_handler,
    .tx_busy         = usb_tx_busy,
    .tx_continue     = usb_tx_continue,
    .tx_idle         = usb_tx_idle,
};
