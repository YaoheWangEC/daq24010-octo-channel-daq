import math
import struct
import time

import numpy as np
import serial
import serial.tools.list_ports

DEVICE_NAME = 'DAQ24010' # device 响应中的设备名特征串
BUFFER_SIZE = 8192 # 采集缓冲区的大小
EEPROM_SIZE = 256 # AT24C02 EEPROM 容量 (字节)
SAMPLERATES = (1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000,
               20000, 30000, 40000, 50000, 60000, 100000, 120000, 200000) # 合法采样率 (Hz)
TRIGGER_MODES = ('software', 'level+', 'level-', 'slope+', 'slope-') # 合法触发模式

CALIB_CHANNELS = 8          # 校正参数通道数
CALIB_STRIDE = 8            # 每通道字节数 (float32 calib_k + float32 calib_b, 小端)
CALIB_BASE_ADDR = 0         # 校正参数在 EEPROM 的起始地址
CALIB_DEFAULT = (1.0, 0.0)  # 默认 (calib_k, calib_b)
CALIB_K_LIMIT = 2.0         # calib_k 允许范围 ±2
CALIB_B_LIMIT = 5.0         # calib_b 允许范围 ±5 (V)


def calib_pack(k: float, b: float) -> bytes:
    """把一阶校正参数打包为 8 字节 (float32 k + float32 b, 小端)。"""
    return struct.pack('<ff', k, b)


def calib_unpack(data: bytes) -> tuple[float, float]:
    """从 8 字节解出一阶校正参数 (calib_k, calib_b)。"""
    return struct.unpack('<ff', data)


def calib_valid(k: float, b: float) -> bool:
    """判断一阶校正参数是否有效 (有限且 |k|<=CALIB_K_LIMIT, |b|<=CALIB_B_LIMIT)。"""
    return (math.isfinite(k) and math.isfinite(b)
            and abs(k) <= CALIB_K_LIMIT and abs(b) <= CALIB_B_LIMIT)


class DAQ24010:
    """DAQ24010 八通道通用采集卡 Python 驱动。

    下位机: AT32F415 + XADC128S022 (12bit 8 通道), RS485 命令面, 无独立数据口
    (命令与数据共用同一串口)。协议为 ASCII 命令、以 CRLF 结尾、一问一答单飞行;
    采集数据经 'adc read' 以紧凑 hex 文本回传 (每样本 4 字符)。

    采集模型为"突发到 8192 点环形缓冲后自动停止", 主机需轮询取数; 受 115200
    文本 hex 链路限制, 连续吞吐上限约 2.88 kSPS。

    用法示例::

        from daq24010 import DAQ24010

        dev = DAQ24010()                 # 自动嗅探串口
        print(dev.get_device_name(), dev.get_uid())

    说明: 串口对象打开后须显式 close() 释放 (或使用 with); close() 默认会发
    reboot 强制设备复位, 使其回到干净上电态。
    """

    # ---- 串口实例 (由 __init__ 打开; pyserial Serial 对象; 关闭不置空, 用 _closed 标记) ----
    ser: serial.Serial   # 命令/数据口 (同一 RS485 串口)
    _closed = False      # 是否已 close(): 此后句柄已关, 不应再收发

    # ---- 设备参数 (由 device 命令响应刷新; 每次 get_*() 现读) ----
    device_name = None   # 设备名, 如 'DAQ24010 octo-channel-universal-daq'
    uid = None           # 24 位十六进制大写 UID; 同型号多台设备靠它区分

    # ---- 配置/状态参数 (status 命令回读缓存, 非权威; 由 get_*/set_*/get_status 刷新) ----
    channel = None            # 当前逻辑通道 1~8
    raw_rate = None           # 片外 ADC 原始采样率 Hz (100k/120k/200k)
    dec_rate = None           # 降采样输出采样率 Hz, 取值 SAMPLERATES
    trigger_mode = None       # 触发模式: software/level+/level-/slope+/slope-
    trigger_threshold = None  # 触发阈值电压 V (设备内部按 12bit 码值比较)
    avdd = None               # 当前 AVDD (ADC 基准/供电) 电压 V, 由 TL431 基准反推

    # ---- 校正参数 (初始化时从 EEPROM 载入; 8 通道, 每项 (calib_k, calib_b)) ----
    _calib = None

    # ==============================
    # 生命周期
    # ==============================

    def __init__(self, port: str | None = None, load_calib: bool = True) -> None:
        """初始化设备对象并打开串口。

        Args:
            port (str | None): 串口名 (如 'COM5'); None 则自动嗅探。
            load_calib (bool): True (默认) 从 EEPROM 载入各通道校正参数;
                False 跳过 (用于 EEPROM 尚未初始化的引导写入场景)。

        Raises:
            RuntimeError: 自动嗅探未找到 DAQ24010; 或 load_calib 且校正参数非法。
            serial.SerialException: 显式指定的串口无法打开。
        """
        explicit = port is not None
        if port is None:
            port = self._probe_port()
            if port is None:
                raise RuntimeError('cannot find any daq24010 port')
        self.ser = serial.Serial(port, 115200, timeout=1)
        if explicit and not self._validate_device():
            self.ser.close()
            self._closed = True
            raise RuntimeError(f'not a DAQ24010 device on {port}')
        if load_calib:
            self._load_calibration()   # 从 EEPROM 载入各通道校正参数

    def close(self, reboot: bool = True) -> None:
        """释放串口资源, 可重复调用 (第二次起为空操作)。

        默认先发 reboot 强制设备复位, 使其回到干净上电态 (会丢失当前
        channel/rate/trigger 配置, 但 EEPROM 内容保留); 复位成功无回包,
        故只发送不等待。句柄保持引用 (不置 None), 用 _closed 标记"已释放";
        关闭失败仅静默 (串口可能已被外部断开)。

        Args:
            reboot (bool): True (默认) 关闭前强制复位设备; False 仅关闭串口。
        """
        if self._closed:
            return
        # getattr 兜底: __init__ 中途失败被析构时字段可能尚未赋值
        ser = getattr(self, 'ser', None)
        if ser is not None:
            if reboot:
                try:
                    self.reboot()
                except Exception:
                    pass
            try:
                ser.close()
            except Exception:
                pass
        self._closed = True

    def __enter__(self) -> 'DAQ24010':
        """支持 with 语句使用: 进入 with 块时返回实例自身。"""
        return self

    def __exit__(self, *exc) -> None:
        """支持 with 语句使用: 块结束 (含异常) 自动释放串口资源。

        返回 None (假值), 使 with 块中未捕获的异常正常向外传播。
        """
        self.close()

    def __del__(self) -> None:
        """析构兜底: 对象被回收时若串口仍打开则释放。

        仅作保险, 不保证执行时机 (交互环境/循环引用下可能很晚才跑);
        确定性释放请用 close() 或 with。异常一律吞掉。
        """
        try:
            self.close()
        except Exception:
            pass

    # ==============================
    # 设备信息
    # ==============================

    def get_device_name(self) -> str:
        """读取设备名, 并刷新设备名/UID 缓存。

        现发 device 命令 (多行响应), 解析出 Name/UID 字段后存入
        self.device_name / self.uid 并返回设备名。

        Returns:
            str: 设备名, 如 'DAQ24010 octo-channel-universal-daq'。

        Raises:
            RuntimeError: 响应中不含 DEVICE_NAME 特征串 (非本设备)。
        """
        resp = self._send('device', resp_single_line=False)
        name_field, uid = self._parse_device_response(resp)
        if DEVICE_NAME not in name_field:
            raise RuntimeError(f'unexpected device response: {resp!r}')
        self.device_name = name_field
        self.uid = uid
        return self.device_name

    def get_uid(self) -> str:
        """读取设备 UID, 并刷新设备名/UID 缓存。

        现发 device 命令 (多行响应), 解析出 Name/UID 字段后存入
        self.device_name / self.uid 并返回 UID。

        Returns:
            str: 24 位十六进制大写 UID; 同型号多台设备靠它区分。

        Raises:
            RuntimeError: 响应中不含 DEVICE_NAME 特征串 (非本设备)。
        """
        resp = self._send('device', resp_single_line=False)
        name_field, uid = self._parse_device_response(resp)
        if DEVICE_NAME not in name_field:
            raise RuntimeError(f'unexpected device response: {resp!r}')
        self.device_name = name_field
        self.uid = uid
        return self.uid

    # ==============================
    # 状态查询
    # ==============================

    def get_avdd(self) -> float:
        """读取当前 AVDD (ADC 基准/供电) 电压 (status avdd 命令)。

        现发 status avdd 命令 (单行响应), 解析为浮点并刷新 self.avdd。

        Returns:
            float: AVDD 电压 (V), 由片内 TL431 基准反推。

        Raises:
            RuntimeError: 响应无法解析为浮点数。
        """
        resp = self._send('status avdd')
        try:
            value = float(resp.strip())
        except ValueError:
            raise RuntimeError(f'unexpected avdd response: {resp!r}')
        self.avdd = value
        return value

    def get_status(self) -> dict:
        """读取配置与状态全量快照 (status 命令, 多行响应)。

        解析后刷新全部配置/状态缓存 (channel / raw_rate / dec_rate /
        trigger_mode / trigger_threshold / avdd)。

        Returns:
            dict: 键 tick / channel / raw_rate / dec_rate / trigger_mode /
                trigger_threshold (V) / state / avdd。

        Raises:
            RuntimeError: 响应字段缺失或无法解析。
        """
        text = self._send('status', resp_single_line=False)
        kv = self._parse_kv_lines(text)
        try:
            trigger_mode, trigger_code = self._split_trigger(kv['trigger'])
            result = {
                'tick': int(kv['tick']),
                'channel': int(kv['channel']),
                'raw_rate': int(kv['raw_rate']),
                'dec_rate': int(kv['dec_rate']),
                'trigger_mode': trigger_mode,
                'trigger_threshold': trigger_code,   # 暂存码值, 待 avdd 就绪后转电压
                'state': kv['state'],
                'avdd': float(kv['avdd']),
            }
        except (KeyError, ValueError):
            raise RuntimeError(f'unexpected status response: {text!r}')

        self.avdd = result['avdd']                   # 先缓存 avdd 与通道, 供换算/校正
        self.channel = result['channel']
        result['trigger_threshold'] = float(self._codes_to_volts(result['trigger_threshold']))
        self.raw_rate = result['raw_rate']
        self.dec_rate = result['dec_rate']
        self.trigger_mode = result['trigger_mode']
        self.trigger_threshold = result['trigger_threshold']
        return result

    # ==============================
    # 配置
    # ==============================

    def set_channel(self, ch: int) -> int:
        """设置采集逻辑通道 (status channel 命令, 仅 IDLE)。

        Args:
            ch (int): 逻辑通道 1~8。

        Returns:
            int: 设置后的逻辑通道 (设备回显)。

        Raises:
            ValueError: ch 不在 1~8。
            RuntimeError: 采集非空闲 (设备回 ERR: busy) 或响应无法解析。
        """
        if ch not in range(1, 9):
            raise ValueError(f'invalid channel {ch}, must be 1~8')
        resp = self._send(f'status channel {ch}')
        try:
            self.channel = self._parse_int_value(resp)
        except ValueError:
            raise RuntimeError(f'unexpected channel response: {resp!r}')
        return self.channel

    def get_channel(self) -> int:
        """查询当前采集逻辑通道 (status channel 命令)。

        Returns:
            int: 逻辑通道 1~8。

        Raises:
            RuntimeError: 响应无法解析。
        """
        resp = self._send('status channel')
        try:
            self.channel = self._parse_int_value(resp)
        except ValueError:
            raise RuntimeError(f'unexpected channel response: {resp!r}')
        return self.channel

    def set_rate(self, hz: int) -> int:
        """设置降采样输出采样率 (status rate 命令, 仅 IDLE)。

        设备按 200→120→100 ksps 优先级自动选定能整除的最大原始采样率,
        故 raw_rate 可能随之改变。

        Args:
            hz (int): 目标输出采样率, 须属于 SAMPLERATES。

        Returns:
            int: 设置后的输出采样率 (dec_rate)。

        Raises:
            ValueError: hz 不在 SAMPLERATES。
            RuntimeError: 采集非空闲 (ERR: busy) 或响应无法解析。
        """
        if hz not in SAMPLERATES:
            raise ValueError(f'invalid samplerate {hz}, legal: {SAMPLERATES}')
        text = self._send(f'status rate {hz}', resp_single_line=False)
        kv = self._parse_kv_lines(text)
        try:
            self.raw_rate = int(kv['raw_rate'])
            self.dec_rate = int(kv['dec_rate'])
        except (KeyError, ValueError):
            raise RuntimeError(f'unexpected rate response: {text!r}')
        return self.dec_rate

    def get_rate(self) -> int:
        """查询当前降采样输出采样率 (status rate 命令)。

        查询只返回 dec_rate, 故仅刷新 dec_rate (raw_rate 保持旧值)。

        Returns:
            int: 输出采样率 (dec_rate)。

        Raises:
            RuntimeError: 响应无法解析。
        """
        resp = self._send('status rate')
        try:
            self.dec_rate = self._parse_int_value(resp)
        except ValueError:
            raise RuntimeError(f'unexpected rate response: {resp!r}')
        return self.dec_rate

    def set_trigger(self, mode: str, threshold: float = 0.0) -> tuple[str, float]:
        """设置触发方式与阈值 (status trigger 命令, 仅 IDLE)。

        Args:
            mode (str): 触发模式, 须属于 TRIGGER_MODES。
            threshold (float): 触发阈值电压 V (电平值或斜率幅度), 内部换算为
                12bit 码值后下发。

        Returns:
            tuple[str, float]: (触发模式, 阈值电压 V)。

        Raises:
            ValueError: mode 非法, 或 threshold 换算后的码值超出 0~4095。
            RuntimeError: 采集非空闲 (ERR: busy) 或响应无法解析。
        """
        if mode not in TRIGGER_MODES:
            raise ValueError(f'invalid trigger mode {mode!r}, legal: {TRIGGER_MODES}')
        code = int(round(float(self._volts_to_code(threshold))))
        if not 0 <= code <= 4095:
            raise ValueError(f'trigger threshold {threshold} V out of range (code {code})')
        resp = self._send(f'status trigger {mode} {code}')
        try:
            self.trigger_mode, trigger_code = self._split_trigger(resp)
        except ValueError:
            raise RuntimeError(f'unexpected trigger response: {resp!r}')
        self.trigger_threshold = float(self._codes_to_volts(trigger_code))
        return self.trigger_mode, self.trigger_threshold

    def get_trigger(self) -> tuple[str, float]:
        """查询当前触发方式与阈值 (status trigger 命令)。

        Returns:
            tuple[str, float]: (触发模式, 阈值电压 V)。

        Raises:
            RuntimeError: 响应无法解析。
        """
        resp = self._send('status trigger')
        try:
            self.trigger_mode, trigger_code = self._split_trigger(resp)
        except ValueError:
            raise RuntimeError(f'unexpected trigger response: {resp!r}')
        self.trigger_threshold = float(self._codes_to_volts(trigger_code))
        return self.trigger_mode, self.trigger_threshold

    # ==============================
    # 采集控制
    # ==============================

    def start(self) -> None:
        """启动采集 (adc start 命令)。

        设备内部会先强制停止当前采集再重启, 并清空采集环形缓冲; 软件触发立即
        进入采集中, 硬件触发进入等待触发。启动后可用 get_state() 查实时状态。
        """
        self._send('adc start')

    def stop(self) -> None:
        """停止采集 (adc stop 命令, 幂等)。

        优雅停止: 先停节拍、等当前帧收尾、停 DMA、CS 拉高; 环形缓冲数据保留。
        """
        self._send('adc stop')

    def get_state(self) -> str:
        """查询采集状态 (adc status 命令)。

        Returns:
            str: 'idle' | 'wait' (等待触发) | 'acq' (采集中)。

        Raises:
            RuntimeError: 响应不是已知状态。
        """
        state = self._send('adc status').strip()
        if state not in ('idle', 'wait', 'acq'):
            raise RuntimeError(f'unexpected state response: {state!r}')
        return state

    def get_acq_info(self) -> dict:
        """读取采集配置与缓冲信息全量快照 (adc 命令, 多行响应)。

        解析后刷新配置/状态缓存 (channel / raw_rate / dec_rate /
        trigger_mode / trigger_threshold / avdd)。

        Returns:
            dict: 键 state / channel / raw_rate / dec_rate / trigger_mode /
                trigger_threshold (V) / avdd / buf_cnt / buf_cap / buf_full /
                overflow。

        Raises:
            RuntimeError: 响应字段缺失或无法解析。
        """
        text = self._send('adc', resp_single_line=False)
        kv = self._parse_kv_lines(text)
        try:
            trigger_mode, trigger_code = self._split_trigger(kv['trigger'])
            result = {
                'state': kv['state'],
                'channel': int(kv['channel']),
                'raw_rate': int(kv['raw_rate']),
                'dec_rate': int(kv['dec_rate']),
                'trigger_mode': trigger_mode,
                'trigger_threshold': trigger_code,   # 暂存码值, 待 avdd 就绪后转电压
                'avdd': float(kv['avdd']),
                'buf_cnt': int(kv['buf_cnt']),
                'buf_cap': int(kv['buf_cap']),
                'buf_full': kv['buf_full'].strip().lower() == 'yes',
                'overflow': int(kv['overflow']),
            }
        except (KeyError, ValueError):
            raise RuntimeError(f'unexpected adc response: {text!r}')

        self.avdd = result['avdd']                   # 先缓存 avdd 与通道, 供换算/校正
        self.channel = result['channel']
        result['trigger_threshold'] = float(self._codes_to_volts(result['trigger_threshold']))
        self.raw_rate = result['raw_rate']
        self.dec_rate = result['dec_rate']
        self.trigger_mode = result['trigger_mode']
        self.trigger_threshold = result['trigger_threshold']
        return result

    def remaining(self) -> int:
        """查询环形缓冲中待读取的样本数 (adc rest 命令)。

        Returns:
            int: 剩余样本数 (0 ~ BUFFER_SIZE)。

        Raises:
            RuntimeError: 响应无法解析。
        """
        resp = self._send('adc rest')
        try:
            return self._parse_int_value(resp)
        except ValueError:
            raise RuntimeError(f'unexpected rest response: {resp!r}')

    # ==============================
    # 数据面
    # ==============================

    def stream_read(self, n: int = 128) -> np.ndarray:
        """读取一次采集数据 (adc read 命令), 返回电压数组。

        单次取数, 不保证连续性; 连续采集由调用方在外层循环调用本方法实现
        (配合 start()/stop())。无数据时 (如硬件触发等待中、环形缓冲已读空)
        返回空数组。

        Args:
            n (int): 本次最多读取的样本数, 1~128 (固件上限)。

        Returns:
            np.ndarray: 电压数组 (float64), 长度 0~n。

        Raises:
            ValueError: n 不在 1~128。
            RuntimeError: 响应无法解析。
        """
        if not 1 <= n <= 128:
            raise ValueError(f'n must be 1~128, got {n}')
        resp = self._send(f'adc read {n}')
        return self._codes_to_volts(self._parse_hex_codes(resp))

    def capture(self, n: int, timeout: float = 5.0) -> np.ndarray:
        """一键采集 n 个样本并返回 (start → 循环取数 → stop)。

        内部先 start(), 再循环 stream_read 攒够 n 点, 最后 (无论成败) stop()。
        低采样率下可连续取满任意长度; 高采样率下受环形缓冲 (BUFFER_SIZE) 限制,
        采满即设备自停, 只能取到已缓存的部分。

        Args:
            n (int): 期望采集的样本数 (>0)。
            timeout (float): 整体超时秒数 (默认 5.0)。硬件触发等待、长采集等
                场景请由调用方按需放大。

        Returns:
            np.ndarray: 电压数组 (float64), 长度恰为 n。

        Raises:
            ValueError: n 非正数。
            RuntimeError: 超时, 或设备提前停止导致取不满 n (不返回短数据)。
        """
        if n <= 0:
            raise ValueError(f'n must be positive, got {n}')
        if timeout <= 0:
            raise ValueError(f'timeout must be positive, got {timeout}')

        self.start()
        end = time.monotonic() + timeout
        chunks: list[np.ndarray] = []
        got = 0
        try:
            while got < n:
                if time.monotonic() >= end:
                    raise RuntimeError(f'capture timeout: got {got}/{n} samples')
                chunk = self.stream_read(min(128, n - got))
                if chunk.size:
                    chunks.append(chunk)
                    got += int(chunk.size)
                elif self.get_state() == 'idle':
                    raise RuntimeError(f'capture stopped early: got {got}/{n} samples')
                else:
                    time.sleep(0.001)
        finally:
            self.stop()
        return np.concatenate(chunks)

    # ==============================
    # 持久化
    # ==============================

    def eeprom_read(self, addr: int, length: int) -> bytes:
        """读取 AT24C02 EEPROM 的连续字节。

        设备单次读取上限 16 字节, 本方法自动分片拼接。

        Args:
            addr (int): 起始地址 0~255。
            length (int): 读取字节数 (>=0), 须满足 addr + length <= EEPROM_SIZE。

        Returns:
            bytes: 读取到的 length 个字节 (length 为 0 时返回空 bytes)。

        Raises:
            ValueError: addr/length 越界。
            RuntimeError: 设备读失败或响应异常。
        """
        if addr < 0 or addr > 255:
            raise ValueError(f'invalid eeprom addr {addr}, must be 0~255')
        if length < 0 or addr + length > EEPROM_SIZE:
            raise ValueError(f'invalid length {length} at addr {addr} '
                             f'(addr+len > {EEPROM_SIZE})')

        out = bytearray()
        pos, remain = addr, length
        while remain > 0:
            n = min(16, remain)
            resp = self._send(f'eeprom read {pos} {n}')
            try:
                out += bytes.fromhex(resp.strip())
            except ValueError:
                raise RuntimeError(f'unexpected eeprom read response: {resp!r}')
            pos += n
            remain -= n
        if len(out) != length:
            raise RuntimeError(f'eeprom read short: got {len(out)}/{length} bytes')
        return bytes(out)

    def eeprom_write(self, addr: int, data: bytes) -> None:
        """写入 AT24C02 EEPROM 的连续字节。

        设备单次写入上限 4 字节, 本方法自动分片。

        Args:
            addr (int): 起始地址 0~255。
            data (bytes): 待写字节 (可为空), 须满足 addr + len(data) <= EEPROM_SIZE。

        Raises:
            ValueError: addr/长度越界。
            RuntimeError: 设备写失败。
        """
        data = bytes(data)
        if addr < 0 or addr > 255:
            raise ValueError(f'invalid eeprom addr {addr}, must be 0~255')
        if addr + len(data) > EEPROM_SIZE:
            raise ValueError(f'invalid length {len(data)} at addr {addr} '
                             f'(addr+len > {EEPROM_SIZE})')

        pos = addr
        for i in range(0, len(data), 4):
            chunk = data[i:i + 4]
            self._send(f'eeprom write {pos} {chunk.hex()}')
            pos += len(chunk)

    # ==============================
    # 其他控制
    # ==============================

    def reboot(self) -> None:
        """强制设备复位 (reboot 命令)。

        设备经 NRST 强制复位, 成功时不返回任何响应, 故本方法只发送命令、
        不等待回包; 复位会丢失当前 channel/rate/trigger 配置 (EEPROM 保留),
        复位后设备约需数百 ms 重新初始化 (期间执行一次上电默认采集)。

        Raises:
            RuntimeError: 设备已 close()。
        """
        if self._closed:
            raise RuntimeError('device is closed')
        self.ser.reset_input_buffer()
        self.ser.write(b'reboot\r\n')
        self.ser.flush()
        time.sleep(0.1)   # 给 USB-RS485 转接器留出最后一帧发送时间

    # ==============================
    # 私有实现
    # ==============================

    def _send(self, cmd: str, quiet: float = 0.05, timeout: float = 1.0,
              resp_single_line: bool = True, raise_on_error: bool = True) -> str:
        """发送一条命令并返回完整响应文本 (不含末尾 CRLF)。

        收帧: 写入后短轮询 in_waiting 收集数据。单行响应 (resp_single_line=True)
        见到尾 CRLF 立即收帧 (响应只含一个 CRLF, 不会误判); 多行响应须等尾
        CRLF 后再静默 quiet 秒, 以区分"第一行结束"与"整段结束"。超过 timeout
        秒仍无完整响应则抛 TimeoutError。

        Args:
            cmd (str): 命令字符串 (不含 CR/LF, ASCII, <=63B)。
            quiet (float): 多行响应判定"整段结束"所需的静默窗 (秒)。
            timeout (float): 总超时 (秒)。
            resp_single_line (bool): 响应是否单行 (默认 True); 多行命令须传 False。
            raise_on_error (bool): True 时命中设备错误回显抛 RuntimeError;
                False 则原样返回文本。

        Returns:
            str: 完整响应文本 (去末尾 CRLF, 保留内部换行)。

        Raises:
            TypeError: cmd 非 str。
            ValueError: cmd 含 CR/LF、非 ASCII 或超过 63B。
            TimeoutError: 超时无完整响应。
            RuntimeError: raise_on_error 且响应命中设备错误回显。
        """
        if self._closed:
            raise RuntimeError('device is closed')
        if not isinstance(cmd, str):
            raise TypeError('cmd must be str')
        if '\r' in cmd or '\n' in cmd:
            raise ValueError(f'cmd must not contain CR/LF: {cmd!r}')
        try:
            payload = cmd.encode('ascii')
        except UnicodeEncodeError:
            raise ValueError(f'cmd must be ascii: {cmd!r}')
        if len(payload) > 63:
            raise ValueError(f'cmd too long ({len(payload)}B > 63): {cmd!r}')

        self.ser.reset_input_buffer()   # 丢弃可能存在的残留字节
        self.ser.write(payload + b'\r\n')

        buf = bytearray()
        last_arrive = time.monotonic()
        t0 = last_arrive
        while True:
            n = self.ser.in_waiting
            if n:
                buf += self.ser.read(n)
                last_arrive = time.monotonic()
                if resp_single_line and buf.endswith(b'\r\n'):
                    break                   # 单行: 读完见尾 CRLF 立即收
            elif buf.endswith(b'\r\n') and (time.monotonic() - last_arrive) >= quiet:
                break                       # 多行: 尾 CRLF + 静默窗确认无后续
            if time.monotonic() - t0 >= timeout:
                raise TimeoutError(f'no response for cmd {cmd!r} (got {bytes(buf)!r})')
            time.sleep(0.001)

        text = bytes(buf).decode('ascii', errors='replace')
        if text.endswith('\r\n'):
            text = text[:-2]
        if raise_on_error and self._looks_like_device_error(text):
            raise RuntimeError(f'device rejected command {cmd!r}: {text}')
        return text

    def _codes_to_volts(self, codes: np.ndarray,
                        calib_k: float | None = None,
                        calib_b: float | None = None) -> np.ndarray:
        """12bit 码值 → 输入电压 (含一阶校正)。

        硬件链路: 输入经衰减/抬升后进 ADC (满量程 0~AVDD), 换算回输入:
            输入电压 = 4 × V_adc - 2 × AVDD,  其中 V_adc = code × AVDD / 4096
        满量程约 ±2×AVDD (约 ±6V)。再叠加一阶校正: V = calib_k × V_raw + calib_b。

        Args:
            codes (np.ndarray): 12bit 码值数组 (0~4095)。
            calib_k (float | None): 一阶增益校正; None 用当前通道的校正值。
            calib_b (float | None): 一阶偏置校正 V; None 用当前通道的校正值。

        Returns:
            np.ndarray: 输入电压数组 (float64)。
        """
        calib_k, calib_b = self._resolve_calib(calib_k, calib_b)
        avdd = self.avdd if self.avdd is not None else self.get_avdd()
        v_adc = np.asarray(codes, dtype=np.float64) * avdd / 4096.0
        return calib_k * (4.0 * v_adc - 2.0 * avdd) + calib_b

    def _volts_to_code(self, volts: np.ndarray,
                       calib_k: float | None = None,
                       calib_b: float | None = None) -> np.ndarray:
        """输入电压 → 12bit 码值 (_codes_to_volts 的逆运算, 含一阶校正)。

        由 V = calib_k × ((code - 2048) × AVDD / 1024) + calib_b 反解:
            code = (V - calib_b) × 1024 / (calib_k × AVDD) + 2048

        Args:
            volts (np.ndarray): 输入电压 (数组或标量)。
            calib_k (float | None): 一阶增益校正; None 用当前通道的校正值。
            calib_b (float | None): 一阶偏置校正 V; None 用当前通道的校正值。

        Returns:
            np.ndarray: 码值 (float, 未取整)。
        """
        calib_k, calib_b = self._resolve_calib(calib_k, calib_b)
        avdd = self.avdd if self.avdd is not None else self.get_avdd()
        return (np.asarray(volts, dtype=np.float64) - calib_b) * 1024.0 / (calib_k * avdd) + 2048.0

    def _resolve_calib(self, calib_k: float | None,
                       calib_b: float | None) -> tuple[float, float]:
        """补全校正参数: 任一为 None 时取当前所选通道的校正值。

        Args:
            calib_k (float | None): 显式增益校正, None 表示用通道校正。
            calib_b (float | None): 显式偏置校正, None 表示用通道校正。

        Returns:
            tuple[float, float]: (calib_k, calib_b)。
        """
        if calib_k is None or calib_b is None:
            ck, cb = self._channel_calib()
            calib_k = ck if calib_k is None else calib_k
            calib_b = cb if calib_b is None else calib_b
        return calib_k, calib_b

    def _channel_calib(self) -> tuple[float, float]:
        """返回当前所选通道的 (calib_k, calib_b)。

        通道缓存未知时现查; 校正未加载或通道越界时返回默认 (1.0, 0.0)。

        Returns:
            tuple[float, float]: 该通道的一阶校正参数。
        """
        ch = self.channel
        if ch is None:
            ch = self.get_channel()
        calib = getattr(self, '_calib', None)
        if calib is not None and 1 <= ch <= CALIB_CHANNELS:
            return calib[ch - 1]
        return CALIB_DEFAULT

    def _load_calibration(self) -> None:
        """从 EEPROM 头部载入 8 通道一阶校正参数。

        每通道 8 字节 (float32 k + float32 b, 小端)。任一项非有限或超出允许
        范围 (|k|<=CALIB_K_LIMIT, |b|<=CALIB_B_LIMIT) 即抛 RuntimeError;
        读取失败亦抛出 (EEPROM 未初始化时 0xFF 解出 NaN, 会命中范围校验)。

        Raises:
            RuntimeError: EEPROM 读取失败, 或某通道校正参数非法/越界。
        """
        data = self.eeprom_read(CALIB_BASE_ADDR, CALIB_CHANNELS * CALIB_STRIDE)
        calib: list[tuple[float, float]] = []
        for i in range(CALIB_CHANNELS):
            off = i * CALIB_STRIDE
            k, b = calib_unpack(data[off:off + CALIB_STRIDE])
            if not calib_valid(k, b):
                raise RuntimeError(
                    f'invalid calibration for CH{i + 1}: k={k}, b={b} '
                    f'(need |k|<={CALIB_K_LIMIT}, |b|<={CALIB_B_LIMIT}); '
                    f'run write_calib.py to initialize EEPROM')
            calib.append((k, b))
        self._calib = calib

    def _validate_device(self) -> bool:
        """向已打开串口连发两次 device, 校验是否为本设备。

        命中 DEVICE_NAME 特征则记录设备名/UID 并返回 True, 否则 False。

        Returns:
            bool: True 表示本设备。
        """
        resp = self._probe_device(self.ser)
        name_field, uid = self._parse_device_response(resp)
        if DEVICE_NAME in name_field:
            self.device_name = name_field
            self.uid = uid
            return True
        return False

    def _probe_port(self) -> str | None:
        """扫描所有串口, 用 device 命令嗅探 DAQ24010 并返回其口名。

        逐个打开候选口, 每个口连发两次 device: 第一次用于对齐 RS485 链路、
        清掉插入瞬间的杂波, 以第二次的响应判定; 命中 DEVICE_NAME 特征即返回
        该口名并记录设备名/UID。候选口无法打开或不应答则跳过。

        Returns:
            str | None: 命中的口名 (如 'COM5'); 未找到则 None。
        """
        for port_info in serial.tools.list_ports.comports():
            name = port_info.device
            try:
                tmp_ser = serial.Serial(name, 115200, timeout=0.2)
            except serial.SerialException:
                continue

            try:
                resp = self._probe_device(tmp_ser)
            finally:
                tmp_ser.close()

            name_field, uid = self._parse_device_response(resp)
            if DEVICE_NAME in name_field:
                self.device_name = name_field
                self.uid = uid
                return name

        return None

    @staticmethod
    def _probe_device(ser: serial.Serial) -> str:
        """向候选口连发两次 device, 返回第二次的响应文本。

        RS485 转接器插入瞬间可能引入杂波被主机当作数据, 第一次 device 用于
        对齐/清链路; 之后清空接收缓冲再发第二次, 取第二次响应作为可靠结果。
        临时把读超时压到 0.2s, 避免在长超时串口上阻塞。

        Args:
            ser (serial.Serial): 已打开的候选串口。

        Returns:
            str: 第二次 device 的响应文本 (ASCII, 非法字节替换)。
        """
        prev_timeout = ser.timeout
        ser.timeout = 0.2
        try:
            ser.reset_input_buffer()
            ser.write(b'device\r\n')
            time.sleep(0.1)
            ser.reset_input_buffer()   # 丢弃第一次响应与插入杂波
            ser.write(b'device\r\n')
            time.sleep(0.1)
            return ser.read(512).decode('ascii', errors='replace')
        finally:
            ser.timeout = prev_timeout

    # ==============================
    # 静态解析辅助
    # ==============================

    @staticmethod
    def _looks_like_device_error(text: str) -> bool:
        """判断响应是否为固件错误回显。

        命中条件 (均为固件自己输出的错误文本前缀):
          - 'ERR:'              越界/非法/忙/失败等
          - 'Unknown command:'  未注册命令
          - 'Invalid '          参数/选项错误 (args/option 等)
          - 'Usage:'            用法错误 (如 eeprom write 缺参)
        正常回显 (裸值/多行 key:value/hex 数据) 不会命中。

        Args:
            text (str): 一条命令的完整响应文本。

        Returns:
            bool: True 表示命中设备错误回显。
        """
        low = text.strip().lower()
        return (low.startswith('err:')
                or low.startswith('unknown command:')
                or low.startswith('invalid ')
                or low.startswith('usage:'))

    @staticmethod
    def _parse_hex_codes(text: str) -> np.ndarray:
        """解析 adc read 的紧凑 hex 响应为 12bit 码值数组。

        Args:
            text (str): 紧凑 hex 文本 (每样本 4 字符, 无分隔)。

        Returns:
            np.ndarray: uint16 码值数组; 空文本返回空数组。

        Raises:
            RuntimeError: hex 长度非 4 的倍数或含非法字符。
        """
        text = text.strip()
        if not text:
            return np.empty(0, dtype=np.uint16)
        try:
            raw = bytes.fromhex(text)
        except ValueError:
            raise RuntimeError(f'malformed hex sample response: {text!r}')
        if len(raw) % 2 != 0:
            raise RuntimeError(f'malformed hex sample response: {text!r}')
        return np.frombuffer(raw, dtype='>u2').astype(np.uint16)

    @staticmethod
    def _parse_int_value(text: str) -> int:
        """解析单值响应为整数 (兼容 'key : value' 与裸 'value')。

        Args:
            text (str): 单行响应文本。

        Returns:
            int: 解析出的整数。

        Raises:
            ValueError: 无法解析为整数。
        """
        if ':' in text:
            text = text.split(':', 1)[1]
        return int(text.strip())

    @staticmethod
    def _parse_kv_lines(text: str) -> dict[str, str]:
        """把多行 'key : value' 响应解析为 dict (键/值去空白)。

        Args:
            text (str): 多行响应文本 (如 status / adc 全量)。

        Returns:
            dict[str, str]: 键到值的映射; 无冒号的行忽略。
        """
        kv: dict[str, str] = {}
        for line in text.split('\r\n'):
            if ':' in line:
                key, val = line.split(':', 1)
                kv[key.strip()] = val.strip()
        return kv

    @staticmethod
    def _split_trigger(value: str) -> tuple[str, int]:
        """解析触发字段为 (模式, 阈值)。

        兼容两种来源: 查询响应的裸 'software @ 0', 与设置响应的
        'trigger  : software @ 0' (含 key 前缀, 自动去除)。

        Args:
            value (str): 形如 '<mode> @ <threshold>' 或 'trigger : <mode> @ <threshold>'。

        Returns:
            tuple[str, int]: (触发模式串, 阈值码值)。

        Raises:
            ValueError: 格式不含 '@' 或阈值非整数。
        """
        if ':' in value:
            value = value.split(':', 1)[1]
        if '@' not in value:
            raise ValueError(f'cannot parse trigger field: {value!r}')
        mode, thr = value.split('@', 1)
        return mode.strip(), int(thr.strip())

    @staticmethod
    def _parse_device_response(resp_text: str) -> tuple[str, str]:
        """解析 device 响应文本, 返回 (设备名, UID)。

        Args:
            resp_text (str): device 命令响应文本。

        Returns:
            tuple[str, str]: (设备名, UID); 未找到对应行时为空串。
        """
        name_field = ''
        uid = ''
        for line in resp_text.split('\r\n'):
            line = line.strip()
            if line.lower().startswith('name'):
                name_field = line.split(':', 1)[1].strip()
            elif line.upper().startswith('UID'):
                uid = line.split(':', 1)[1].strip()
        return name_field, uid


if __name__ == '__main__':
    """命令面冒烟自检: 打开设备, 校验 device 命令与 _send 收帧/错误判定。

    直接运行: python daq24010.py
    """
    with DAQ24010() as dev:
        # 设备信息 (device 命令, 多行响应)
        name = dev.get_device_name()
        uid = dev.get_uid()
        print(f'device : {name}')
        print(f'uid    : {uid}')
        assert DEVICE_NAME in name, name
        assert len(uid) == 24, uid

        # 初始化时已从 EEPROM 载入各通道校正参数
        print(f'calib  : {dev._calib}')
        assert len(dev._calib) == CALIB_CHANNELS

        # 状态查询 (status avdd 单行 / status 全量多行)
        avdd = dev.get_avdd()
        print(f'avdd   : {avdd:.3f} V')
        assert 2.6 <= avdd <= 3.6, avdd

        st = dev.get_status()
        print(f'status : {st}')
        assert st['channel'] in range(1, 9), st
        assert st['state'] in ('idle', 'wait', 'acq'), st
        assert st['raw_rate'] >= st['dec_rate'] > 0, st
        assert 2.6 <= st['avdd'] <= 3.6, st

        # get_status 应刷新配置/状态缓存
        assert dev.channel == st['channel'], (dev.channel, st['channel'])
        assert dev.raw_rate == st['raw_rate'], (dev.raw_rate, st['raw_rate'])
        assert dev.dec_rate == st['dec_rate'], (dev.dec_rate, st['dec_rate'])
        assert dev.trigger_mode == st['trigger_mode'], (dev.trigger_mode, st['trigger_mode'])
        assert dev.trigger_threshold == st['trigger_threshold']
        assert dev.avdd == st['avdd']

        # 配置 (先停采集确保 IDLE)
        dev._send('adc stop')
        assert dev.set_channel(3) == 3 and dev.channel == 3
        assert dev.get_channel() == 3
        assert dev.set_rate(8000) == 8000 and dev.dec_rate == 8000
        assert dev.get_rate() == 8000
        assert dev.raw_rate in (100000, 120000, 200000), dev.raw_rate
        mode, thr = dev.set_trigger('level+', 0.0)
        assert mode == 'level+' and abs(thr) < 0.02, (mode, thr)
        assert dev.get_trigger() == (mode, thr)

        # 采集控制
        dev.set_trigger('software', 0)
        dev.stop()                        # 幂等
        assert dev.get_state() == 'idle'
        dev.start()
        info = dev.get_acq_info()
        print(f'acq    : {info}')
        assert info['state'] in ('idle', 'wait', 'acq'), info
        assert info['buf_cap'] == BUFFER_SIZE, info
        assert 0 <= info['buf_cnt'] <= BUFFER_SIZE, info
        assert isinstance(info['buf_full'], bool), info
        assert dev.get_state() in ('idle', 'wait', 'acq')
        assert 0 <= dev.remaining() <= BUFFER_SIZE
        dev.stop()
        assert dev.get_state() == 'idle'

        # 数据面: 一键采集 (内部 start→stream_read→stop)
        dev.set_trigger('software', 0)
        volts = dev.capture(512, timeout=5.0)
        print(f'capture: shape={volts.shape}, '
              f'range=[{volts.min():.3f}, {volts.max():.3f}] V')
        assert volts.shape == (512,), volts.shape
        assert np.isfinite(volts).all()

        # _send 收帧: echo 单行响应
        assert dev._send('echo hello') == 'hello'

        # _send 错误判定: 未知命令应抛 RuntimeError
        try:
            dev._send('nosuchcmd')
        except RuntimeError:
            pass
        else:
            raise AssertionError('unknown command should raise RuntimeError')

        # _send 参数校验: 含 CR/LF 应抛 ValueError
        try:
            dev._send('echo a\rb')
        except ValueError:
            pass
        else:
            raise AssertionError('CRLF command should raise ValueError')

        # 持久化: 写入/回读 (会改写 EEPROM 末尾 16 字节)
        pat = bytes(range(16))
        dev.eeprom_write(240, pat)
        back = dev.eeprom_read(240, 16)
        print(f'eeprom : wrote {pat.hex()} read {back.hex()}')
        assert back == pat, (pat, back)

        import matplotlib.pyplot as plt

        # 高速突发: 200kSPS 采 8000 点 (环 8192 约 41ms 填满自停, 再读出)
        dev.set_channel(1)
        dev.set_rate(200000)
        dev.set_trigger('software', 0)
        t0 = time.monotonic()
        burst = dev.capture(8000, timeout=10.0)
        dt = time.monotonic() - t0
        print(f'burst  : n={burst.size} in {dt:.2f}s, '
              f'min={burst.min():+.4f} max={burst.max():+.4f} V')
        assert burst.shape == (8000,), burst.shape
        assert np.isfinite(burst).all()

        plt.figure()
        plt.plot(np.arange(burst.size) / 200000 * 1000.0, burst, linewidth=0.5)
        plt.title('200 kSPS burst (8000 pts)')
        plt.xlabel('time (ms)')
        plt.ylabel('voltage (V)')
        plt.grid(True)
        plt.tight_layout()

        # 触发突发: 0V 电平上升沿触发, 采一个完整周期 (10Hz → 60kSPS 下 6000 点)
        sig_freq = 10.0
        rate = 60000
        n_period = int(rate / sig_freq)
        dev.set_channel(1)
        dev.set_rate(rate)
        dev.set_trigger('level+', 0.0)
        t0 = time.monotonic()
        one = dev.capture(n_period, timeout=10.0)
        dt = time.monotonic() - t0
        print(f'trig   : n={one.size} in {dt:.2f}s, '
              f'min={one.min():+.4f} max={one.max():+.4f} V')
        assert one.shape == (n_period,), one.shape

        plt.figure()
        plt.plot(np.arange(one.size) / rate * 1000.0, one, linewidth=0.8)
        plt.title(f'0V rising trigger, 1 period @ {rate // 1000} kSPS')
        plt.xlabel('time (ms)')
        plt.ylabel('voltage (V)')
        plt.grid(True)
        plt.tight_layout()

        # 连续采集 @ 2kSPS (0V 电平上升触发); duration 按需调整
        fs = 2000
        duration = 5.0
        n = int(fs * duration)
        dev.set_channel(1)
        dev.set_rate(fs)
        dev.set_trigger('level+', 0.0)

        t0 = time.monotonic()
        wave = dev.capture(n, timeout=duration + 10.0)
        dt = time.monotonic() - t0
        print(f'wave   : n={wave.size} in {dt:.2f}s '
              f'({wave.size / dt:.0f} SPS), '
              f'min={wave.min():+.4f} max={wave.max():+.4f} V')

        plt.figure()
        plt.plot(np.arange(wave.size) / fs, wave, linewidth=0.5)
        plt.title(f'2 kSPS continuous ({duration:.0f} s)')
        plt.xlabel('time (s)')
        plt.ylabel('voltage (V)')
        plt.grid(True)
        plt.tight_layout()

        # 连续信号的频域幅度谱 (Hann 窗, 对峰值归一化的 dB)
        from numpy.fft import rfft, rfftfreq

        win = np.hanning(wave.size)
        spec = np.abs(rfft(wave * win)) * 2.0 / win.sum()
        freqs = rfftfreq(wave.size, d=1.0 / fs)
        fmax = 100.0
        m = freqs <= fmax
        spec_db = 20.0 * np.log10(np.maximum(spec, 1e-12) / spec.max())
        pk = int(np.argmax(spec_db[m]))
        print(f'spectrum: peak {freqs[m][pk]:.2f} Hz, '
              f'amplitude {spec[m][pk]:.3f} V (0 dB)')

        plt.figure()
        plt.plot(freqs[m], spec_db[m])
        plt.title('normalized amplitude spectrum (dB, peak = 0)')
        plt.xlabel('frequency (Hz)')
        plt.ylabel('amplitude (dB)')
        plt.grid(True)
        plt.tight_layout()
        plt.show()

    print('CMD-PLANE SMOKE OK')
