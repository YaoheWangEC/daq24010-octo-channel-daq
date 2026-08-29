#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  单生产者单消费者（SPSC）环形缓冲
 * @note   head 仅生产者写入、tail 仅消费者写入，volatile 保证跨中断可见，
 *         无需关中断即可安全使用。支持任意 uint16_t 长度（无需 2 的幂）。
 */
typedef struct
{
    uint16_t        *buf;           /**< 存储区首地址（由调用方提供，须有 len+1 个元素） */
    uint16_t         len;           /**< 可用容量（元素个数） */
    volatile uint16_t head;         /**< 写指针（生产者） */
    volatile uint16_t tail;         /**< 读指针（消费者） */
    volatile uint32_t overflow_cnt; /**< 满时丢弃并计数 */
} ringbuf_t;

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
void ringbuf_init(ringbuf_t *rb, uint16_t *storage, uint16_t len);

/**
 * @brief  获取当前有效数据个数
 * @param  rb: 环形缓冲实例
 * @retval 当前缓存的数据个数（0 ~ len）
 * @note   O(1) 计算，供消费端/状态显示使用；中断中调用返回读取时刻的瞬时值。
 */
uint16_t ringbuf_len(const ringbuf_t *rb);

/**
 * @brief  判断缓冲是否为空
 * @param  rb: 环形缓冲实例
 * @retval true 缓冲为空（head == tail），无数据可消费
 * @retval false 缓冲非空
 */
bool ringbuf_is_empty(const ringbuf_t *rb);

/**
 * @brief  判断缓冲是否已满
 * @param  rb: 环形缓冲实例
 * @retval true 缓冲已满（(head+1) % (len+1) == tail），push 将失败
 * @retval false 缓冲未满
 */
bool ringbuf_is_full(const ringbuf_t *rb);

/**
 * @brief  压入一个样本（生产者侧）
 * @param  rb:     环形缓冲实例
 * @param  sample: 待压入的样本值
 * @retval true 压入成功
 * @retval false 缓冲已满，样本被丢弃且 overflow_cnt 累加
 * @note   仅修改 head，可在中断中安全调用。满时不覆盖旧数据，配合
 *         ringbuf_overflow_cnt 可检测数据堆积。
 */
bool ringbuf_push(ringbuf_t *rb, uint16_t sample);

/**
 * @brief  弹出一个样本（消费者侧）
 * @param  rb:  环形缓冲实例
 * @param  out: 出参，存放弹出的样本
 * @retval true 弹出成功
 * @retval false 缓冲为空，out 内容不变
 * @note   仅修改 tail，可在中断中安全调用。
 */
bool ringbuf_pop(ringbuf_t *rb, uint16_t *out);

/**
 * @brief  获取溢出丢弃计数
 * @param  rb: 环形缓冲实例
 * @retval 自初始化/复位以来因缓冲已满而丢弃的样本总数
 * @note   只增不减，用于监控生产速率是否长期高于消费速率。
 */
uint32_t ringbuf_overflow_cnt(const ringbuf_t *rb);

/**
 * @brief  复位环形缓冲（清空数据与溢出计数）
 * @param  rb: 环形缓冲实例
 * @warning 须保证与生产/消费侧无并发访问（如采集已停止）时调用。
 */
void ringbuf_reset(ringbuf_t *rb);

#endif
