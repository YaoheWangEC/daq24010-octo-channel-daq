# DAQ24010 Python 驱动

DAQ24010 八通道通用采集卡（AT32F415 + XADC128S022，RS485 命令面）的 Python 上位机驱动。

## 文件

| 文件 | 说明 |
|---|---|
| `daq24010.py` | 驱动主体（`DAQ24010` 类） + `__main__` 冒烟自检 |
| `write_calib.py` | 向 EEPROM 写入 8 通道一阶校正参数 |

## 依赖

- Python 3.10+（使用 `X | None` 类型标注）
- `pyserial`、`numpy`；自检绘图另需 `matplotlib`

```
pip install pyserial numpy matplotlib
```

## 快速开始

```python
from daq24010 import DAQ24010

with DAQ24010() as dev:                 # 自动嗅探串口（也可 DAQ24010('COM5')）
    print(dev.get_device_name(), dev.get_uid())

    dev.set_channel(1)
    dev.set_rate(2000)
    dev.set_trigger('level+', 0.0)      # 0V 电平上升沿触发（阈值单位: V）
    v = dev.capture(2000, timeout=5.0)  # 一键采集 2000 点, 返回电压 np.ndarray

# with 退出时 close(): 默认发 reboot 复位设备
```

连续采集（自行控制启停）：

```python
dev.start()
try:
    while ...:
        chunk = dev.stream_read(128)    # 单次取数, 返回电压数组（可能为空）
finally:
    dev.stop()
```

## API 速查

| 区域 | 方法 |
|---|---|
| 生命周期 | `__init__(port=None, load_calib=True)` / `close(reboot=True)` / `with` |
| 设备信息 | `get_device_name()` / `get_uid()` |
| 状态查询 | `get_avdd()` / `get_status()` |
| 配置（仅 IDLE） | `set_channel`/`get_channel`、`set_rate`/`get_rate`、`set_trigger`/`get_trigger` |
| 采集控制 | `start` / `stop` / `get_state` / `get_acq_info` / `remaining` |
| 数据面 | `stream_read(n=128)` / `capture(n, timeout=5.0)` |
| 持久化 | `eeprom_read(addr, length)` / `eeprom_write(addr, data)` |
| 其他 | `reboot()` |

## 采集模型与吞吐

- 设备采集写入 **8192 点环形缓冲**，**写满即自动停止**（不循环覆盖）；主机需轮询 `stream_read` 排空。
- 115200 + 4 hex 字符/样本 → 连续吞吐上限约 **2.7~2.88 kSPS**。
  - **rate ≤ ~2.5k**：主机排空快于采样 → 可长时间连续采集。
  - **rate > ~2.8k**：环很快填满自停 → 只能“突发捕获”最多 8192 点后再读出。
- `capture(n)` 的循环天然兼容两种模式；取不满 n 抛 `RuntimeError`（不返回短数据）。
- 协议无样本序号/CRC，无法验证连续性。

## 触发

- 模式：`software` / `level+` / `level-` / `slope+` / `slope-`（`TRIGGER_MODES`）。
- `set_trigger(mode, threshold)` 的 `threshold` 为**电压 V**；`level±` 为电平、`slope±` 为相邻样本差值幅度。
- 判定基于降采样后的输出样本（2 点窗口）。
- `level±` / `slope±` 属硬件触发，设备侧尚未上板实测。
- 固件无触发超时；等待触发时 `stream_read` 返回空数组，`capture` 需给足 `timeout`。

## 电压换算与校正

硬件链路：输入经衰减/抬升后进 ADC（满量程 0~AVDD），换算回输入：

```
V_adc = code × AVDD / 4096
V_in  = calib_k × (4 × V_adc − 2 × AVDD) + calib_b     # 满量程约 ±2·AVDD (≈ ±6.6V)
```

- AVDD 由 TL431 基准实时反推（`get_avdd`）。
- 校正按**通道**生效：初始化时从 EEPROM 自动载入 8 通道 `(calib_k, calib_b)`；换算时按当前所选通道自动套用。
- 范围校验：`|k| ≤ 2`、`|b| ≤ 5`；越界或读取失败在构造时抛 `RuntimeError`。

### EEPROM 校正布局（从地址 0 起，每通道 8 字节，小端 float32）

| 偏移 | 内容 |
|---|---|
| `ch*8 + 0` | `calib_k` (float32) |
| `ch*8 + 4` | `calib_b` (float32) |

写入（默认 1/0；实际校准后改 `write_calib.py` 的 `CALIB` 表重跑）：

```
python write_calib.py
```

> 空 EEPROM 会解出 NaN 导致构造失败，脚本用 `DAQ24010(load_calib=False)` 绕过校验完成引导写入。

## 命令协议速查

命令以 `\r\n` 结尾，一问一答，ASCII。错误响应以 `ERR:` / `Unknown command:` / `Invalid ` / `Usage:` 开头（驱动统一转 `RuntimeError`）。

| 命令 | 说明 |
|---|---|
| `device` | 设备名 + UID |
| `status` / `status <field>` / `status channel\|rate\|trigger <v>` | 配置/状态查询与修改（改仅 IDLE） |
| `adc` / `adc status\|start\|stop\|rest\|read [n]` | 采集控制与数据读取 |
| `eeprom read <addr> [len]` / `eeprom write <addr> <hex>` | EEPROM 读写 |
| `reboot` | 强制复位（成功无回包） |

## 自检

```
python daq24010.py
```

覆盖设备信息、状态、配置、采集控制、数据面（突发/触发/连续/频谱）、EEPROM、`_send` 收帧与错误判定。需连接硬件；会改写 EEPROM 末尾 16 字节。
