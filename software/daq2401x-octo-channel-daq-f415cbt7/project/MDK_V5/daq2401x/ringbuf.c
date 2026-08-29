#include "ringbuf.h"

/**
 * @brief  内部环形大小
 * @param  rb: 环形缓冲实例
 * @retval len + 1（多出的 1 个元素用于区分空/满）
 */
static uint32_t ringbuf_size(const ringbuf_t *rb)
{
    return (uint32_t)rb->len + 1u;
}

/**
 * @brief  初始化环形缓冲
 * @param  rb:      环形缓冲实例
 * @param  storage: 存储区首地址
 * @param  len:     可用容量（元素个数，任意 uint16_t，1 ~ 0xFFFE）
 * @note   实际传入的缓冲区应提供 len+1 个元素，多出的 1 个用于区分
 *         空/满（满 = (head+1) % (len+1) == tail），因此可用容量恰好为 len。
 *         内部索引按 (len+1) 取模，长度无 2 的幂限制。
 * @warning 仅可在缓冲无并发访问（采集已停止）时调用，否则会丢失未读数据。
 */
void ringbuf_init(ringbuf_t *rb, uint16_t *storage, uint16_t len)
{
    rb->buf = storage;
    rb->len = len;
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_cnt = 0;
}

/**
 * @brief  获取当前有效数据个数
 * @param  rb: 环形缓冲实例
 * @retval 当前缓存的数据个数（0 ~ len）
 * @note   O(1) 计算，供消费端/状态显示使用；中断中调用返回读取时刻的瞬时值。
 */
uint16_t ringbuf_len(const ringbuf_t *rb)
{
    uint32_t size = ringbuf_size(rb);

    /* 用 32 位中间运算计算 (head - tail + size) % size，避免 len 接近上限时溢出 */
    return (uint16_t)(((uint32_t)rb->head + size - (uint32_t)rb->tail) % size);
}

/**
 * @brief  判断缓冲是否为空
 * @param  rb: 环形缓冲实例
 * @retval true 缓冲为空（head == tail），无数据可消费
 * @retval false 缓冲非空
 */
bool ringbuf_is_empty(const ringbuf_t *rb)
{
    return (rb->head == rb->tail);
}

/**
 * @brief  判断缓冲是否已满
 * @param  rb: 环形缓冲实例
 * @retval true 缓冲已满（(head+1) % (len+1) == tail），push 将失败
 * @retval false 缓冲未满
 */
bool ringbuf_is_full(const ringbuf_t *rb)
{
    uint32_t size = ringbuf_size(rb);

    return (((uint32_t)rb->head + 1u) % size) == (uint32_t)rb->tail;
}

/**
 * @brief  压入一个样本（生产者侧）
 * @param  rb:     环形缓冲实例
 * @param  sample: 待压入的样本值
 * @retval true 压入成功
 * @retval false 缓冲已满，样本被丢弃且 overflow_cnt 累加
 * @note   仅修改 head，可在中断中安全调用。满时不覆盖旧数据，配合
 *         ringbuf_overflow_cnt 可检测数据堆积。
 */
bool ringbuf_push(ringbuf_t *rb, uint16_t sample)
{
    uint32_t size = ringbuf_size(rb);
    uint32_t next = ((uint32_t)rb->head + 1u) % size;

    if (next == (uint32_t)rb->tail)
    {
        rb->overflow_cnt++;
        return false;
    }

    rb->buf[rb->head] = sample;
    rb->head = (uint16_t)next;
    return true;
}

/**
 * @brief  弹出一个样本（消费者侧）
 * @param  rb:  环形缓冲实例
 * @param  out: 出参，存放弹出的样本
 * @retval true 弹出成功
 * @retval false 缓冲为空，out 内容不变
 * @note   仅修改 tail，可在中断中安全调用。
 */
bool ringbuf_pop(ringbuf_t *rb, uint16_t *out)
{
    uint32_t size;

    if (rb->head == rb->tail)
    {
        return false;
    }

    *out = rb->buf[rb->tail];
    size = ringbuf_size(rb);
    rb->tail = (uint16_t)(((uint32_t)rb->tail + 1u) % size);
    return true;
}

/**
 * @brief  获取溢出丢弃计数
 * @param  rb: 环形缓冲实例
 * @retval 自初始化/复位以来因缓冲已满而丢弃的样本总数
 * @note   只增不减，用于监控生产速率是否长期高于消费速率。
 */
uint32_t ringbuf_overflow_cnt(const ringbuf_t *rb)
{
    return rb->overflow_cnt;
}

/**
 * @brief  复位环形缓冲（清空数据与溢出计数）
 * @param  rb: 环形缓冲实例
 * @warning 须保证与生产/消费侧无并发访问（如采集已停止）时调用。
 */
void ringbuf_reset(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_cnt = 0;
}
