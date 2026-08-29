#include "bsp.h"
#include "global_vars.h"
#include "at32f415_wdt.h"
#include "at32f415_wk_config.h"
#include "wk_dma.h"
#include "wk_system.h"
#include "rgbled.h"
#include "i2c_app.h"
#include "eeprom.h"
#include "key.h"

// 重定义 WorkBench 弱实现：长延时期间喂狗（WDT 超时 3.28s，50ms 步进远小于超时）
// 覆盖所有长时间等待场景（LCD 自检、初始化 wait 等）
void wk_delay_ms(uint32_t delay)
{
    while (delay)
    {
        wdt_counter_reload();   // 喂狗
        if (delay > 50)
        {
            wk_delay_us(50 * 1000);
            delay -= 50;
        }
        else
        {
            wk_delay_us(delay * 1000);
            delay = 0;
        }
    }
}

// 启动后丢弃的降采样输出样本数（复用 trigger_prev 负值区做倒计数）
#define ADC_STARTUP_DISCARD_SAMPLES 100

// 降采样内部状态
static uint16_t decimate_factor = 1;      // 降采样因子 N = 原始速率/目标速率
static uint32_t decimate_accumulator;     // 平均累加器
static uint16_t decimate_count;           // 已累积原始样本数

// 触发比较基准：<0 时为丢弃倒计数（每次收到输出样本 +1）；≥0 时为上一降采样样本
static int16_t trigger_prev = -ADC_STARTUP_DISCARD_SAMPLES;

// VREF 反推 AVDD：片内 ADC1 连续转换 + DMA1_CH6 采集 TL431 基准（2.495V）
#define VREF_DMA_SAMPLES 64
#define VREF_DUMMY_CODE 3097   // 上电预填码值（≈2.495V@3.3V 名义值），首个测量完成前占位
#define VREF_AVDD_MIN_V 2.6f   // AT32F415 工作电压下限（反推结果低于此值判定无效）
#define VREF_AVDD_MAX_V 3.6f   // AT32F415 工作电压上限（反推结果高于此值判定无效）

static uint16_t vref_dma_buf[VREF_DMA_SAMPLES];

// AT24C02 EEPROM 参数（I2C2 外挂，A0A1A2 接地）
#define AT24C02_ADDR            0xA0    // 8 位从机地址
#define AT24C02_SIZE            256     // 容量 256 字节
#define AT24C02_TIMEOUT         1000000 // I2C 标志等待超时循环数

// 内部静态函数
static inline uint16_t adc128s022_cmd(uint8_t addr);
static uint32_t tmr3_clk_hz_get(void);
static bool adc_is_running(void);
static bool adc_raw_samplerate_set(adc_raw_samplerate_t samplerate);
static bool adc_decimate_push(uint16_t raw_data, uint16_t *decimated_data);
static bool adc_trigger_check(uint16_t sample);
static void adc_process_rx_half(const uint16_t *buf, uint16_t count);

/**
 * @brief  板级初始化总入口
 */
void bsp_init(void)
{
    // 预填 VREF 采集缓冲，避免上电首次反推使用未初始化值
    for (uint16_t i = 0; i < VREF_DMA_SAMPLES; i++)
    {
        vref_dma_buf[i] = VREF_DUMMY_CODE;
    }

    adc_stop();
    adc_logic_channel_set(ADC_LOGIC_CHANNEL_1);
    adc_decimated_samplerate_set(ADC_DECIMATED_SAMPLERATE_8KSPS);
    adc_trigger_set(ADC_TRIGGER_MODE_SOFTWARE, 0);

    key_add(KEY_GPIO_PORT, KEY_PIN, 0);   // 注册按键 id=0

    vref_start();   // 启动首轮 VREF 采集
}

/**
 * @brief  设置片外 ADC 逻辑通道
 * @param  channel: 逻辑通道编号
 * @note   同步点亮对应通道的 LED 指示灯（高电平点亮）
 */
void adc_logic_channel_set(adc_logic_channel_t channel)
{
    // 熄灭全部通道 LED
    gpio_bits_reset(LED_CH1_GPIO_PORT, LED_CH1_PIN);
    gpio_bits_reset(LED_CH2_GPIO_PORT, LED_CH2_PIN);
    gpio_bits_reset(LED_CH3_GPIO_PORT, LED_CH3_PIN);
    gpio_bits_reset(LED_CH4_GPIO_PORT, LED_CH4_PIN);
    gpio_bits_reset(LED_CH5_GPIO_PORT, LED_CH5_PIN);
    gpio_bits_reset(LED_CH6_GPIO_PORT, LED_CH6_PIN);
    gpio_bits_reset(LED_CH7_GPIO_PORT, LED_CH7_PIN);
    gpio_bits_reset(LED_CH8_GPIO_PORT, LED_CH8_PIN);

    // 点亮所选通道 LED
    switch (channel)
    {
    case ADC_LOGIC_CHANNEL_1:
        gpio_bits_set(LED_CH1_GPIO_PORT, LED_CH1_PIN);
        break;
    case ADC_LOGIC_CHANNEL_2:
        gpio_bits_set(LED_CH2_GPIO_PORT, LED_CH2_PIN);
        break;
    case ADC_LOGIC_CHANNEL_3:
        gpio_bits_set(LED_CH3_GPIO_PORT, LED_CH3_PIN);
        break;
    case ADC_LOGIC_CHANNEL_4:
        gpio_bits_set(LED_CH4_GPIO_PORT, LED_CH4_PIN);
        break;
    case ADC_LOGIC_CHANNEL_5:
        gpio_bits_set(LED_CH5_GPIO_PORT, LED_CH5_PIN);
        break;
    case ADC_LOGIC_CHANNEL_6:
        gpio_bits_set(LED_CH6_GPIO_PORT, LED_CH6_PIN);
        break;
    case ADC_LOGIC_CHANNEL_7:
        gpio_bits_set(LED_CH7_GPIO_PORT, LED_CH7_PIN);
        break;
    case ADC_LOGIC_CHANNEL_8:
        gpio_bits_set(LED_CH8_GPIO_PORT, LED_CH8_PIN);
        break;
    default:
        break;
    }

    g_vars.logic_channel = channel;
}

/**
 * @brief  获取当前片外 ADC 逻辑通道
 * @retval 逻辑通道编号 1~8，异常返回 0
 */
uint8_t adc_logic_channel_get(void)
{
    switch (g_vars.logic_channel)
    {
    case ADC_LOGIC_CHANNEL_1:
        return 1;
    case ADC_LOGIC_CHANNEL_2:
        return 2;
    case ADC_LOGIC_CHANNEL_3:
        return 3;
    case ADC_LOGIC_CHANNEL_4:
        return 4;
    case ADC_LOGIC_CHANNEL_5:
        return 5;
    case ADC_LOGIC_CHANNEL_6:
        return 6;
    case ADC_LOGIC_CHANNEL_7:
        return 7;
    case ADC_LOGIC_CHANNEL_8:
        return 8;
    default:
        return 0;
    }
}

/**
 * @brief  按逻辑通道编号设置片外 ADC 逻辑通道
 * @param  num: 逻辑通道编号 1~8，其余视为 1
 * @note   与 adc_logic_channel_get 对称；同步点亮对应通道的 LED 指示灯
 */
void adc_logic_channel_set_by_num(uint8_t num)
{
    switch (num)
    {
    case 1:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_1);
        break;
    case 2:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_2);
        break;
    case 3:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_3);
        break;
    case 4:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_4);
        break;
    case 5:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_5);
        break;
    case 6:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_6);
        break;
    case 7:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_7);
        break;
    case 8:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_8);
        break;
    default:
        adc_logic_channel_set(ADC_LOGIC_CHANNEL_1);
        break;
    }
}

/**
 * @brief  设置降采样目标输出采样率
 * @param  rate: 目标输出采样率，取值 ADC_DECIMATED_SAMPLERATE_xxx
 * @retval true 配置成功（已自动选定并配置原始采样率）
 *         false 目标速率无效，或采集正在运行，或无可整除的原始采样率
 * @note   仅可在采集停止时调用。按 200→120→100 ksps 优先级选择
 *         最大且能整除的原始采样率，N = 原始速率/目标速率。
 */
bool adc_decimated_samplerate_set(adc_decimated_samplerate_t rate)
{
    static const adc_raw_samplerate_t candidates[] =
    {
        ADC_RAW_SAMPLERATE_200KSPS,
        ADC_RAW_SAMPLERATE_120KSPS,
        ADC_RAW_SAMPLERATE_100KSPS
    };
    adc_raw_samplerate_t raw = ADC_RAW_SAMPLERATE_100KSPS;
    uint16_t n = 0;
    uint32_t i;

    if (adc_is_running())
    {
        return false;
    }

    switch (rate)
    {
    case ADC_DECIMATED_SAMPLERATE_1KSPS:
    case ADC_DECIMATED_SAMPLERATE_2KSPS:
    case ADC_DECIMATED_SAMPLERATE_3KSPS:
    case ADC_DECIMATED_SAMPLERATE_4KSPS:
    case ADC_DECIMATED_SAMPLERATE_5KSPS:
    case ADC_DECIMATED_SAMPLERATE_6KSPS:
    case ADC_DECIMATED_SAMPLERATE_8KSPS:
    case ADC_DECIMATED_SAMPLERATE_10KSPS:
    case ADC_DECIMATED_SAMPLERATE_20KSPS:
    case ADC_DECIMATED_SAMPLERATE_30KSPS:
    case ADC_DECIMATED_SAMPLERATE_40KSPS:
    case ADC_DECIMATED_SAMPLERATE_50KSPS:
    case ADC_DECIMATED_SAMPLERATE_60KSPS:
    case ADC_DECIMATED_SAMPLERATE_100KSPS:
    case ADC_DECIMATED_SAMPLERATE_120KSPS:
    case ADC_DECIMATED_SAMPLERATE_200KSPS:
        break;
    default:
        return false;
    }

    // 从大到小选择能整除的原始采样率（总是最大）
    for (i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); i++)
    {
        if (((uint32_t)candidates[i] % (uint32_t)rate) == 0)
        {
            raw = candidates[i];
            n = (uint16_t)((uint32_t)candidates[i] / (uint32_t)rate);
            break;
        }
    }

    if (n == 0)
    {
        return false;
    }

    if (!adc_raw_samplerate_set(raw))
    {
        return false;
    }

    decimate_factor = n;
    decimate_accumulator = 0;
    decimate_count = 0;
    g_vars.decimated_samplerate = rate;

    return true;
}

/**
 * @brief  查询采集是否运行中（等待触发或采集中均视为运行）
 * @retval true 运行中，false 已停止
 * @note   内部使用，由采集状态机驱动
 */
static bool adc_is_running(void)
{
    return (g_vars.acq_state != ADC_STATE_IDLE);
}

/**
 * @brief  设置片外 ADC 真实采样率
 * @param  samplerate: 目标采样率，取值 ADC_RAW_SAMPLERATE_xxx
 * @retval true 配置成功（TMR3 已更新，全局变量已同步）
 *         false 采样率无效，或采集正在运行
 * @note   仅可在采集停止时调用。TMR3 计数时钟 = 120 MHz，
 *         溢出周期 = 1/采样率，PR = 计数时钟/采样率 - 1，DIV 保持 0。
 */
static bool adc_raw_samplerate_set(adc_raw_samplerate_t samplerate)
{
    uint32_t tmr_clk;
    uint32_t ticks;

    if (adc_is_running())
    {
        return false;
    }

    switch (samplerate)
    {
    case ADC_RAW_SAMPLERATE_100KSPS:
    case ADC_RAW_SAMPLERATE_120KSPS:
    case ADC_RAW_SAMPLERATE_200KSPS:
        break;

    default:
        return false;
    }

    tmr_clk = tmr3_clk_hz_get();
    ticks = tmr_clk / (uint32_t)samplerate;

    if ((ticks < 2) || (ticks > 65536))
    {
        return false;
    }

    tmr_counter_enable(TMR3, FALSE);
    tmr_base_init(TMR3, (uint32_t)(ticks - 1), 0);   // F415 库: (pr, div)

    g_vars.raw_samplerate = samplerate;

    return true;
}

/**
 * @brief  计算 TMR3 计数时钟
 * @retval TMR3 计数时钟频率（Hz）
 * @note   RM 规则：APB1 预分频系数为 1 时 TMR 时钟 = APB1 时钟，否则 ×2
 */
static uint32_t tmr3_clk_hz_get(void)
{
    crm_clocks_freq_type clocks;

    crm_clocks_freq_get(&clocks);

    return (CRM->cfg_bit.apb1div == 0) ? clocks.apb1_freq : (clocks.apb1_freq * 2);
}

/**
 * @brief  设置触发方式与触发阈值
 * @param  mode:      触发方式（见 adc_trigger_mode_t）
 * @param  threshold: 触发阈值，仅使用正值（0~4095）；
 *                    电平触发为电平值，斜率触发为相邻两点差值的幅度
 * @retval true 配置成功
 *         false 采集非空闲（未处于 ADC_STATE_IDLE），或触发方式非法，或阈值越界
 * @note   仅可在 ADC_STATE_IDLE（空闲）时调用。
 */
bool adc_trigger_set(adc_trigger_mode_t mode, int16_t threshold)
{
    if (g_vars.acq_state != ADC_STATE_IDLE)
    {
        return false;
    }

    switch (mode)
    {
    case ADC_TRIGGER_MODE_SOFTWARE:
    case ADC_TRIGGER_MODE_LEVEL_RISING:
    case ADC_TRIGGER_MODE_LEVEL_FALLING:
    case ADC_TRIGGER_MODE_SLOPE_RISING:
    case ADC_TRIGGER_MODE_SLOPE_FALLING:
        break;
    default:
        return false;
    }

    if ((threshold < 0) || (threshold > 4095))
    {
        return false;
    }

    g_vars.trigger_mode = mode;
    g_vars.trigger_threshold = threshold;

    return true;
}

/**
 * @brief  启动 SPI DMA 采集
 * @details F415 弹性映射分配：DMA1_CH3 = SPI1_RX（外设->内存），
 *          DMA1_CH4 = TMR3_OVERFLOW（内存->外设）。TMR3 溢出事件
 *          触发一次 CH4 搬运，将 TX 缓冲中的 16bit 命令字写入
 *          SPI1->dt 发出一帧；CH3 同步将 MISO 数据捕获到 RX 缓冲。
 */
void adc_start(void)
{
    uint16_t cmd;
    uint16_t i;

    if (adc_is_running())
    {
        adc_stop();
    }

    cmd = adc128s022_cmd((uint8_t)g_vars.logic_channel);

    for (i = 0; i < ADC_DMA_BUF_LEN; i++)
    {
        g_vars.adc_dma_tx_buf[i] = cmd;
    }

    // 停 DMA，防止配置期间有残留触发
    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_channel_enable(DMA1_CHANNEL4, FALSE);

    // 清除 SPI 残留状态：读 STS+DT 清 RDBF，ROERR 需显式清除
    (void)SPI1->sts;
    (void)SPI1->dt;
    spi_i2s_flag_clear(SPI1, SPI_I2S_ROERR_FLAG);

    // 清 DMA 通道 3/4 中断标志，避免启动即触发旧中断
    dma_flag_clear(DMA1_FDT3_FLAG);
    dma_flag_clear(DMA1_HDT3_FLAG);
    dma_flag_clear(DMA1_FDT4_FLAG);
    dma_flag_clear(DMA1_HDT4_FLAG);

    // RX: DMA1_CH3 (SPI1_RX) 外设->内存
    wk_dma_channel_config(DMA1_CHANNEL3,
                          (uint32_t)&SPI1->dt,
                          (uint32_t)g_vars.adc_dma_rx_buf,
                          ADC_DMA_BUF_LEN);
    // TX: DMA1_CH4 (TMR3_OVERFLOW) 内存->外设
    wk_dma_channel_config(DMA1_CHANNEL4,
                          (uint32_t)&SPI1->dt,
                          (uint32_t)g_vars.adc_dma_tx_buf,
                          ADC_DMA_BUF_LEN);

    // CS 拉低：唤醒 ADC 并复位其 16 边沿计数器（帧重同步）
    gpio_bits_reset(ADC_NSS_GPIO_PORT, ADC_NSS_PIN);

    // 复位采集数据环形缓冲，避免上次残留数据导致立即判满
    ringbuf_reset(&g_vars.adc_ring);

    // 复位降采样平均状态，丢弃半组残留，保证输出对齐
    decimate_accumulator = 0;
    decimate_count = 0;

    // 复位触发比较基准为丢弃倒计数：丢弃启动阶段前 N 个输出样本
    trigger_prev = -(int16_t)ADC_STARTUP_DISCARD_SAMPLES;
    g_vars.acq_state = (g_vars.trigger_mode == ADC_TRIGGER_MODE_SOFTWARE)
                       ? ADC_STATE_ACQUIRING
                       : ADC_STATE_WAIT_TRIGGER;

    if (g_vars.acq_state == ADC_STATE_ACQUIRING)
    {
        rgbled_set(LED_ON, LED_OFF, LED_OFF);   // 红灯=采集中
    }
    else
    {
        rgbled_set(LED_OFF, LED_OFF, LED_ON);   // 蓝灯=等待触发
    }

    // 先开 RX，再开 TX，最后开 TMR3 节拍
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
    dma_channel_enable(DMA1_CHANNEL4, TRUE);
    tmr_counter_enable(TMR3, TRUE);
}

/**
 * @brief  生成 XADC128S022 控制命令字
 * @param  addr: 通道多路复用地址 0~7
 * @retval 16bit 命令字，控制寄存器位于高 8 位（bit2=1、bit6=0、ADD2~0 选通道）
 */
static inline uint16_t adc128s022_cmd(uint8_t addr)
{
    return (uint16_t)((0x04u | (addr << 3)) << 8);
}

/**
 * @brief  停止 SPI DMA 采集
 * @details 先停 TMR3 节拍，等待当前帧收尾（避免半帧导致 ADC 失步），
 *          再停 DMA，最后 CS 拉高使 ADC 进入掉电。
 */
void adc_stop(void)
{
    tmr_counter_enable(TMR3, FALSE);

    while (spi_i2s_flag_get(SPI1, SPI_I2S_BF_FLAG) != RESET)
    {
    }

    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_channel_enable(DMA1_CHANNEL4, FALSE);

    gpio_bits_set(ADC_NSS_GPIO_PORT, ADC_NSS_PIN);

    g_vars.acq_state = ADC_STATE_IDLE;

    rgbled_set(LED_OFF, LED_ON, LED_OFF);   // 绿灯=空闲
}

/**
 * @brief  启动新一轮 TL431 基准连续采集（片内 ADC1 连续转换 + DMA1_CH6）
 * @details 重装 DMA1_CH6 计数器并指向采集缓冲（方向/宽度/非循环由 WorkBench
 *          生成配置），软件触发一次后 ADC1 连续转换，DMA 背靠背搬运 64 个。
 * @note   上电由 bsp_init 启动首轮；此后由 1ms 节拍在上一轮收满后调用。
 */
void vref_start(void)
{
    dma_channel_enable(DMA1_CHANNEL6, FALSE);

    dma_flag_clear(DMA1_FDT6_FLAG);
    dma_flag_clear(DMA1_HDT6_FLAG);

    // 重装计数器并指向采集缓冲（方向/宽度/非循环已由 WorkBench 生成配置）
    wk_dma_channel_config(DMA1_CHANNEL6,
                          (uint32_t)&ADC1->odt,
                          (uint32_t)vref_dma_buf,
                          VREF_DMA_SAMPLES);

    dma_channel_enable(DMA1_CHANNEL6, TRUE);

    adc_ordinary_software_trigger_enable(ADC1, TRUE);   // 一次触发，连续转换
}

/**
 * @brief  检测 VREF 上一轮采集是否完成（DMA 已收满 VREF_DMA_SAMPLES 个）
 * @retval true 完成：已停 DMA，缓冲为完整一轮数据
 *         false 仍在采集中
 * @note   由 1ms 节拍调用；完成后调用方反推 AVDD 并启动下一轮。
 */
bool vref_check_done(void)
{
    if (dma_flag_get(DMA1_FDT6_FLAG) == RESET)
    {
        return false;
    }

    dma_channel_enable(DMA1_CHANNEL6, FALSE);

    return true;
}

/**
 * @brief  由 TL431 基准反推 AVDD 并保存到 g_vars.avdd
 * @retval true 反推成功（g_vars.avdd 已更新）
 *         false 平均码值为 0，或反推电压超出 AT32F415 工作电压区间
 *               （结果无效，g_vars.avdd 保持原值不变）
 * @note   由 1ms 节拍在上一轮收满后调用，基于该轮完整采集缓冲反推。
 */
bool vref_derive(void)
{
    uint32_t sum = 0;
    uint16_t i;
    uint16_t n_avg;
    float avdd;

    for (i = 0; i < VREF_DMA_SAMPLES; i++)
    {
        sum += vref_dma_buf[i];
    }

    n_avg = (uint16_t)(sum / VREF_DMA_SAMPLES);
    if (n_avg == 0)
    {
        return false;
    }

    // N = VREF × 4096 / VDDA  =>  VDDA(=AVDD) = VREF × 4096 / N
    avdd = VREF * 4096.0f / (float)n_avg;

    // 反推电压超出 AT32F415 工作电压区间 → 结果无效，不更新
    if ((avdd < VREF_AVDD_MIN_V) || (avdd > VREF_AVDD_MAX_V))
    {
        return false;
    }

    g_vars.avdd = avdd;

    return true;
}

/**
 * @brief  AT24C02 批量读取
 * @param  addr: 起始地址 0~255
 * @param  buf:  读入缓冲区
 * @param  len:  读取字节数
 * @retval true 成功；false 参数越界或 I2C 错误
 * @note   读取无页边界限制，可跨页连续读。
 */
bool at24c02_read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    if (addr + len > AT24C02_SIZE)
    {
        return false;
    }
    if (len == 0)
    {
        return true;
    }

    return (eeprom_read_buffer(&hi2c2, EE_MODE_POLL, I2C_MEM_ADDR_WIDTH_8,
                               AT24C02_ADDR, addr, buf, len,
                               AT24C02_TIMEOUT) == I2C_OK);
}

/**
 * @brief  AT24C02 批量写入
 * @param  addr: 起始地址 0~255
 * @param  buf:  待写数据
 * @param  len:  写入字节数
 * @retval true 成功；false 参数越界或 I2C 错误
 * @note   库内部自动按 8 字节页拆分，并以 ACK 轮询等待写周期结束。
 */
bool at24c02_write(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    if (addr + len > AT24C02_SIZE)
    {
        return false;
    }
    if (len == 0)
    {
        return true;
    }

    return (eeprom_write_buffer(&hi2c2, EE_MODE_POLL, I2C_MEM_ADDR_WIDTH_8,
                                AT24C02_ADDR, addr, (uint8_t *)buf, len,
                                AT24C02_TIMEOUT) == I2C_OK);
}

/**
 * @brief  TX/RX DMA 传输完成一轮回调（后半区 [HALF, LEN) 就绪）
 * @details 处理后半区数据；环形缓冲满则停止采集。
 */
void spi_tx_rx_cplt_handler(void)
{
    adc_process_rx_half(&g_vars.adc_dma_rx_buf[ADC_DMA_BUF_LEN / 2], ADC_DMA_BUF_LEN / 2);
}

/**
 * @brief  TX/RX DMA 传输过半回调（前半区 [0, HALF) 就绪）
 * @details 处理前半区数据；环形缓冲满则停止采集。
 */
void spi_tx_rx_half_cplt_handler(void)
{
    adc_process_rx_half(&g_vars.adc_dma_rx_buf[0], ADC_DMA_BUF_LEN / 2);
}

/**
 * @brief  处理一个 DMA 半区：解码 → 降采样 → 启动丢弃 → 触发判定 → 推入环形缓冲
 * @param  buf:   半区首地址
 * @param  count: 半区样本数（ADC_DMA_BUF_LEN/2）
 * @note   在 DMA 中断中调用。启动丢弃：trigger_prev 为负时每收到一个输出样本
 *         倒计数 +1，丢弃前 N 个输出样本（规避上电瞬态与首样本误触发），
 *         计数归零时保存首个真实样本作比较基准。之后按状态机：
 *         - WAIT_TRIGGER：逐样本判触发，满足后转 ACQUIRING，触发样本入环；
 *         - ACQUIRING：逐样本入环，环满则 adc_stop（存满停止 → IDLE）；
 *         - IDLE：残留中断直接返回。
 */
static void adc_process_rx_half(const uint16_t *buf, uint16_t count)
{
    uint16_t i;
    uint16_t sample;
    uint16_t decimated;

    for (i = 0; i < count; i++)
    {
        sample = (uint16_t)(buf[i] & 0x0FFFu);   // 去掉 4 个前导零，取 12 位
        if (!adc_decimate_push(sample, &decimated))
        {
            continue;
        }

        // 启动丢弃：trigger_prev 为负时每收到一个输出样本 +1，丢弃前 N 个
        if (trigger_prev < 0)
        {
            if (++trigger_prev < 0)
            {
                continue;   // 丢弃该输出样本（不入环、不判触发）
            }
            trigger_prev = (int16_t)decimated;   // 计数归零：保存首个真实样本作基准
            continue;
        }

        switch (g_vars.acq_state)
        {
        case ADC_STATE_WAIT_TRIGGER:
            if (adc_trigger_check(decimated))
            {
                g_vars.acq_state = ADC_STATE_ACQUIRING;
                rgbled_set(LED_ON, LED_OFF, LED_OFF);   // 红灯=采集中
                // 触发样本作为环内首个数据；环满立即停止
                if (!ringbuf_push(&g_vars.adc_ring, decimated))
                {
                    adc_stop();
                    return;
                }
            }
            break;

        case ADC_STATE_ACQUIRING:
            if (!ringbuf_push(&g_vars.adc_ring, decimated))
            {
                adc_stop();   // 缓冲区存满，采集结束
                return;
            }
            break;

        default:   // IDLE：停止后残留中断，直接返回
            return;
        }
    }
}

/**
 * @brief  降采样累积：每 N 个原始样本平均为 1 个输出样本
 * @param  raw_data:       解码后的 12 位原始样本（0x000~0xFFF）
 * @param  decimated_data: 出参，返回 true 时存放平均出的输出样本
 * @retval true 累积满一组 N 个，decimated_data 有效，由调用方推入环形缓冲
 *         false 仍在累积中，decimated_data 无效
 * @note   内部状态跨调用保持，可跨 DMA 缓冲边界对齐，无需缓冲长度是 N 的整数倍。
 *         入环前调用；N=1 时每次调用均产出一个输出（输出=输入）。
 *         adc_start 时复位内部状态，保证输出对齐。
 */
static bool adc_decimate_push(uint16_t raw_data, uint16_t *decimated_data)
{
    decimate_accumulator += raw_data;
    decimate_count++;

    if (decimate_count < decimate_factor)
    {
        return false;
    }

    *decimated_data = (uint16_t)(decimate_accumulator / decimate_factor);
    decimate_accumulator = 0;
    decimate_count = 0;
    return true;
}

/**
 * @brief  触发判定：用当前与上一降采样样本比较（2 点窗口）
 * @param  sample: 当前降采样输出样本
 * @retval true 触发条件满足
 * @note   仅状态为 WAIT_TRIGGER 且已过启动丢弃阶段时调用（trigger_prev ≥ 0）。
 *         判定后更新触发比较基准。SOFTWARE 模式不经过本函数。
 */
static bool adc_trigger_check(uint16_t sample)
{
    int32_t diff;
    bool fired = false;

    switch (g_vars.trigger_mode)
    {
    case ADC_TRIGGER_MODE_LEVEL_RISING:
        fired = (trigger_prev < (uint16_t)g_vars.trigger_threshold) &&
                (sample >= (uint16_t)g_vars.trigger_threshold);
        break;

    case ADC_TRIGGER_MODE_LEVEL_FALLING:
        fired = (trigger_prev > (uint16_t)g_vars.trigger_threshold) &&
                (sample <= (uint16_t)g_vars.trigger_threshold);
        break;

    case ADC_TRIGGER_MODE_SLOPE_RISING:
        diff = (int32_t)sample - (int32_t)trigger_prev;
        fired = (diff >= (int32_t)g_vars.trigger_threshold);
        break;

    case ADC_TRIGGER_MODE_SLOPE_FALLING:
        diff = (int32_t)sample - (int32_t)trigger_prev;
        fired = (diff <= -(int32_t)g_vars.trigger_threshold);
        break;

    default:
        break;
    }

    trigger_prev = (int16_t)sample;
    return fired;
}

/**
 * @brief 1 ms 系统节拍，由 TMR11 溢出中断调用
 */
void tick_handler(void)
{
    g_vars.tick++;

    // FDT6 置位 = 上一轮收满：停 DMA → 反推 AVDD → 立即开始下一轮
    if (vref_check_done())
    {
        vref_derive();
        vref_start();
    }

    if ((g_vars.tick % 10) == 0)
    {
        key_scan(); // 每 10 ms 扫描按键
    }
}
