#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

#include "global_vars.h"

/**
 * @brief  板级初始化总入口
 */
void bsp_init(void);

/**
 * @brief  设置片外 ADC 逻辑通道
 * @param  channel: 逻辑通道编号
 * @note   同步点亮对应通道的 LED 指示灯（高电平点亮）
 */
void adc_logic_channel_set(adc_logic_channel_t channel);

/**
 * @brief  获取当前片外 ADC 逻辑通道
 * @retval 逻辑通道编号 1~8，异常返回 0
 */
uint8_t adc_logic_channel_get(void);

/**
 * @brief  按逻辑通道编号设置片外 ADC 逻辑通道
 * @param  num: 逻辑通道编号 1~8，其余视为 1
 * @note   与 adc_logic_channel_get 对称；同步点亮对应通道的 LED 指示灯
 */
void adc_logic_channel_set_by_num(uint8_t num);

/**
 * @brief  设置降采样目标输出采样率
 * @param  rate: 目标输出采样率，取值 ADC_DECIMATED_SAMPLERATE_xxx
 * @retval true 配置成功（已自动选定并配置原始采样率）
 *         false 目标速率无效，或采集正在运行，或无可整除的原始采样率
 * @note   仅可在采集停止时调用。按 200→120→100 ksps 优先级选择
 *         最大且能整除的原始采样率，N = 原始速率/目标速率。
 */
bool adc_decimated_samplerate_set(adc_decimated_samplerate_t rate);

/**
 * @brief  设置触发方式与触发阈值
 * @param  mode:      触发方式（见 adc_trigger_mode_t）
 * @param  threshold: 触发阈值，仅使用正值（0~4095）；
 *                    电平触发为电平值，斜率触发为相邻两点差值的幅度
 * @retval true 配置成功
 *         false 采集非空闲（未处于 ADC_STATE_IDLE），或触发方式非法，或阈值越界
 * @note   仅可在 ADC_STATE_IDLE（空闲）时调用。
 */
bool adc_trigger_set(adc_trigger_mode_t mode, int16_t threshold);

/**
 * @brief  启动 SPI DMA 采集
 * @details F415 弹性映射分配：DMA1_CH3 = SPI1_RX（外设->内存），
 *          DMA1_CH4 = TMR3_OVERFLOW（内存->外设）。TMR3 溢出事件
 *          触发一次 CH4 搬运，将 TX 缓冲中的 16bit 命令字写入
 *          SPI1->dt 发出一帧；CH3 同步将 MISO 数据捕获到 RX 缓冲。
 */
void adc_start(void);

/**
 * @brief  停止 SPI DMA 采集
 * @details 先停 TMR3 节拍，等待当前帧收尾（避免半帧导致 ADC 失步），
 *          再停 DMA，最后 CS 拉高使 ADC 进入掉电。
 */
void adc_stop(void);

/**
 * @brief  启动新一轮 TL431 基准连续采集（片内 ADC1 连续转换 + DMA1_CH6）
 * @details 重装 DMA1_CH6 计数器并指向采集缓冲，软件触发一次后 ADC1 连续转换，
 *          背靠背搬运 VREF_DMA_SAMPLES 个。上电由 bsp_init 启动首轮。
 */
void vref_start(void);

/**
 * @brief  检测 VREF 上一轮采集是否完成（DMA 已收满 VREF_DMA_SAMPLES 个）
 * @retval true 完成：已停 DMA，缓冲为完整一轮数据
 *         false 仍在采集中
 * @note   由 1ms 节拍调用；完成后调用方反推 AVDD 并启动下一轮。
 */
bool vref_check_done(void);

/**
 * @brief  由 TL431 基准反推 AVDD 并保存到 g_vars.avdd
 * @retval true 反推成功（g_vars.avdd 已更新）
 *         false 平均码值为 0，或反推电压超出 AT32F415 工作电压区间
 *               （结果无效，g_vars.avdd 保持原值不变）
 * @note   由 1ms 节拍在上一轮收满后调用，基于该轮完整采集缓冲反推。
 */
bool vref_derive(void);

/**
 * @brief  AT24C02 批量读取
 * @param  addr: 起始地址 0~255
 * @param  buf:  读入缓冲区
 * @param  len:  读取字节数
 * @retval true 成功；false 参数越界或 I2C 错误
 * @note   读取无页边界限制，可跨页连续读。
 */
bool at24c02_read(uint16_t addr, uint8_t *buf, uint16_t len);

/**
 * @brief  AT24C02 批量写入
 * @param  addr: 起始地址 0~255
 * @param  buf:  待写数据
 * @param  len:  写入字节数
 * @retval true 成功；false 参数越界或 I2C 错误
 * @note   库内部自动按 8 字节页拆分，并以 ACK 轮询等待写周期结束。
 */
bool at24c02_write(uint16_t addr, const uint8_t *buf, uint16_t len);

/**
 * @brief  TX/RX DMA 传输完成一轮回调（后半区 [HALF, LEN) 就绪）
 * @details 处理后半区数据；环形缓冲满则停止采集。
 */
void spi_tx_rx_cplt_handler(void);

/**
 * @brief  TX/RX DMA 传输过半回调（前半区 [0, HALF) 就绪）
 * @details 处理前半区数据；环形缓冲满则停止采集。
 */
void spi_tx_rx_half_cplt_handler(void);

/**
 * @brief 1 ms 系统节拍，由 TMR11 溢出中断调用
 */
void tick_handler(void);

#endif
