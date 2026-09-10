# -*- coding: utf-8 -*-
"""向 DAQ24010 的 AT24C02 EEPROM 写入 8 个通道的一阶校正参数。

布局与驱动一致 (见 daq24010.py 的 CALIB_* 常量):
    从地址 CALIB_BASE_ADDR(=0) 开始, 每通道 CALIB_STRIDE(=8) 字节:
        addr = ch * 8      : calib_k (float32, 小端)
        addr = ch * 8 + 4  : calib_b (float32, 小端)
8 通道 × 8 字节 = 64 字节。

当前尚未真正校准, 写入默认值 (k=1.0, b=0.0)。实际校准后改 CALIB 表重跑即可。

用法 (在驱动目录下): python write_calib.py
"""

from daq24010 import (DAQ24010, CALIB_BASE_ADDR, CALIB_CHANNELS,
                      CALIB_STRIDE, calib_pack, calib_unpack, calib_valid)

# 每通道 (calib_k, calib_b); 暂用默认值
CALIB: list[tuple[float, float]] = [(1.0, 0.0)] * CALIB_CHANNELS


def pack_calib(calib: list[tuple[float, float]]) -> bytes:
    """把各通道校正参数打包为 EEPROM 字节流。

    Args:
        calib: 每通道 (calib_k, calib_b) 列表。

    Returns:
        bytes: 按通道顺序拼接的字节流 (每通道 CALIB_STRIDE 字节, 小端 float32)。
    """
    return b''.join(calib_pack(k, b) for k, b in calib)


def main() -> None:
    """写入校正参数并回读校验。"""
    for ch, (k, b) in enumerate(CALIB, start=1):
        assert calib_valid(k, b), f'CH{ch} calibration out of range: k={k}, b={b}'

    data = pack_calib(CALIB)
    assert len(data) == CALIB_CHANNELS * CALIB_STRIDE

    with DAQ24010(load_calib=False) as dev:
        dev.eeprom_write(CALIB_BASE_ADDR, data)
        back = dev.eeprom_read(CALIB_BASE_ADDR, len(data))
        assert back == data, (data.hex(), back.hex())

        for ch in range(CALIB_CHANNELS):
            off = ch * CALIB_STRIDE
            k, b = calib_unpack(back[off:off + CALIB_STRIDE])
            print(f'CH{ch + 1}: calib_k={k:.6f}, calib_b={b:.6f}')

    print('CALIB WRITE OK')


if __name__ == '__main__':
    main()
