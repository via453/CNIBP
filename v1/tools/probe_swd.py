"""不接串口，直接通过 SWD 读 RAM 里的计数器，验证下位机三条链路的真实帧率。

前置：固件里必须有 g_dbg_* 计数器（v1 已内置），且 .axf 与本机烧录的固件一致。

用法：
    D:\\microsoft_download\\anaconda3\\Anaconda\\python.exe ^
        C:\\Users\\Zelvia\\cnibp.staging\\v1\\tools\\probe_swd.py

判定标准（与 v1/docs/protocol.md 一致）：
    ECG  240-260 /s      （ADS1292R 250 SPS）
    PPG   95-105 /s      （MAX30102 100 Hz）
    TEMP 0.8-1.2 /s      （1 Hz）
"""
import os
import re
import subprocess
import sys
import time

AXF = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'firmware', 'ads1292_rtos', 'build', 'PhysioCard_v1.axf')
FROMELF = r'D:\keil5\ARM\ARMCC\bin\fromelf.exe'
TARGET = 'stm32f103rc'

WANT = ['xTickCount', 'g_dbg_ecg_frames', 'g_dbg_ppg_frames', 'g_dbg_temp_frames',
        'g_dbg_loops', 'g_dbg_stage', 'g_ads1292_recv_flag',
        'fifo_ir', 'fifo_red', 'g_CurrentTemp', 'g_ads1292_cache']


def symbols():
    """从 axf 里提取符号地址。"""
    out = subprocess.run([FROMELF, '--text', '-s', AXF],
                         capture_output=True, text=True, errors='ignore').stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3 or not parts[2].startswith('0x'):
            continue
        name = parts[1]
        if name in WANT:
            syms[name] = int(parts[2], 16)
    return syms


def u32(t, a):
    return int.from_bytes(t.read_memory_block8([a])[0:0] or b'', 'little')


def read32(target, addr):
    return int.from_bytes(bytes(target.read_memory_block8(addr, 4)), 'little')


def read16(target, addr):
    return int.from_bytes(bytes(target.read_memory_block8(addr, 2)), 'little')


def read8(target, addr):
    return int.from_bytes(bytes(target.read_memory_block8(addr, 1)), 'little')


def main():
    from pyocd.core.helpers import ConnectHelper

    syms = symbols()
    missing = [k for k in WANT if k not in syms]
    if missing:
        print('缺少符号（.axf 与固件不一致？）:', missing)
        return 2

    session = ConnectHelper.session_with_chosen_probe(
        target_override=TARGET, options={'frequency': 4000000})
    if session is None:
        print('没有可用探针')
        return 2

    with session:
        t = session.target
        t.resume()

        tick0 = read32(t, syms['xTickCount'])
        s0 = {k: read32(t, syms[k]) for k in
              ('g_dbg_ecg_frames', 'g_dbg_ppg_frames', 'g_dbg_temp_frames', 'g_dbg_loops')}

        print('采样 10 秒 …')
        time.sleep(10)

        tick1 = read32(t, syms['xTickCount'])
        s1 = {k: read32(t, syms[k]) for k in s0}
        dt = (tick1 - tick0) / 1000.0
        if dt <= 0:
            print('节拍没有前进：SysTick 可能又被关掉了（dt=%.3f）' % dt)
            return 1

        stage = read32(t, syms['g_dbg_stage'])
        cache = bytes(t.read_memory_block8(syms['g_ads1292_cache'], 9))
        ir = read16(t, syms['fifo_ir'])
        red = read16(t, syms['fifo_red'])
        temp = struct_unpack_f32(t, syms['g_CurrentTemp'])
        ecg_flag = read8(t, syms['g_ads1292_recv_flag'])

    print()
    print('时间基准：tick 差值 %.3f s（1 kHz）' % dt)
    print('%-8s %10s %10s %10s %s' % ('链路', '差值', '速率/s', '期望/s', '判定'))
    limits = {'g_dbg_ecg_frames': (240, 260, 250),
              'g_dbg_ppg_frames': (95, 105, 100),
              'g_dbg_temp_frames': (0.8, 1.2, 1)}
    ok = True
    for k, (lo, hi, exp) in limits.items():
        n = s1[k] - s0[k]
        rate = n / dt
        good = lo <= rate <= hi
        ok = ok and good
        print('%-8s %10d %10.1f %10d %s' % (k.replace('g_dbg_', '').replace('_frames', ''),
                                            n, rate, exp, 'PASS' if good else 'FAIL'))
    print()
    print('主循环速率   : %.1f /s' % ((s1['g_dbg_loops'] - s0['g_dbg_loops']) / dt))
    print('初始化阶段号 : %d  （9 = 已进入主循环）' % stage)
    print('ADS1292 缓存 : %s' % cache.hex(' '))
    print('ECG 接收标志 : %d' % ecg_flag)
    print('fifo_ir/red  : %d / %d' % (ir, red))
    print('体温         : %.2f ℃' % temp)
    print()
    print('ALL PASS' if ok else 'SOME CHECKS FAILED')
    return 0 if ok else 1


def struct_unpack_f32(target, addr):
    import struct
    return struct.unpack('<f', bytes(target.read_memory_block8(addr, 4)))[0]


if __name__ == '__main__':
    sys.exit(main())
