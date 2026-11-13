"""PPG2ABP 路线桥接：100 Hz PPG → UNetDS64 → MultiResUNet1D → ABP → SBP/DBP/MAP。

与 ABPImputation 路线的区别：**只要 PPG，不需要 ECG**。适合干电极心电质量不达标时使用。

用法：
    :: 离线：拿 CSV（t_ms,ecg_raw,ppg_ir）跑一次
    python ppg2abp_bridge.py --csv capture.csv

    :: 在线：直接读串口 0xBB 帧的 IR 通道
    python ppg2abp_bridge.py --port COM9 --window 8.2

模型默认从原工程目录加载（255 MB，不建议进 Git）：
    --model-dir 指定，或设环境变量 PPG2ABP_HOME
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
import time

import numpy as np

TARGET_LENGTH = 1024      # 模型输入长度
TARGET_RATE = 125         # 模型采样率
SENSOR_RATE = 100         # 固件 PPG 采样率
RAW_LENGTH = int(round(TARGET_LENGTH * SENSOR_RATE / TARGET_RATE))   # 820

DEFAULT_HOME = r'G:\Class_Design\Class_Design\fork_BP\Projects\PPG2ABP\codes'

_models = {}


def resolve_home(model_dir: str | None) -> str:
    d = model_dir or os.environ.get('PPG2ABP_HOME') or DEFAULT_HOME
    if not os.path.isdir(d):
        raise FileNotFoundError(
            f'找不到 PPG2ABP 工程目录：{d}\n'
            f'用 --model-dir 或环境变量 PPG2ABP_HOME 指定（需含 models/*.h5 与 models.py）')
    return d


def load_models(model_dir: str | None = None):
    """加载两个权重（只做一次）。"""
    if _models:
        return _models

    home = resolve_home(model_dir)
    if home not in sys.path:
        sys.path.insert(0, home)

    import pickle
    import warnings
    warnings.filterwarnings('ignore')

    from models import UNetDS64, MultiResUNet1D

    m1 = UNetDS64(TARGET_LENGTH)
    m1.load_weights(os.path.join(home, 'models', 'ApproximateNetwork.h5'))

    m2 = MultiResUNet1D(TARGET_LENGTH)
    m2.load_weights(os.path.join(home, 'models', 'RefinementNetwork.h5'))

    meta_path = os.path.join(home, 'data', 'meta9.p')
    meta = pickle.load(open(meta_path, 'rb')) if os.path.exists(meta_path) else {
        'min_abp': 50.0, 'max_abp': 200.0}

    _models.update({'mdl1': m1, 'mdl2': m2, 'meta': meta, 'home': home})
    print(f'[ppg2abp] 模型已加载：{os.path.join(home, "models")}')
    print(f'[ppg2abp] 反归一化区间：[{meta["min_abp"]:.1f}, {meta["max_abp"]:.1f}] mmHg')
    return _models


def bandpass(data, fs=SENSOR_RATE, lowcut=0.5, highcut=5.0, order=3):
    from scipy.signal import butter, filtfilt
    nyq = 0.5 * fs
    b, a = butter(order, [lowcut / nyq, min(highcut / nyq, 0.99)], btype='band')
    return filtfilt(b, a, np.asarray(data, dtype=np.float64))


def resample_to(ppg, target_len=TARGET_LENGTH):
    """线性插值到 target_len（等价于 100 Hz → 125 Hz 重采样）。"""
    x_old = np.linspace(0.0, 1.0, num=len(ppg))
    x_new = np.linspace(0.0, 1.0, num=target_len)
    return np.interp(x_new, x_old, ppg).astype(np.float32)


def predict_bp(ppg_100hz: np.ndarray, model_dir: str | None = None):
    """输入 100 Hz PPG（建议 820 点 = 8.2 s），输出 (sbp, dbp, map, abp_waveform)。"""
    m = load_models(model_dir)
    ppg = np.asarray(ppg_100hz, dtype=np.float64)
    if ppg.size < 50:
        raise ValueError('PPG 样本太少')

    ppg_f = bandpass(ppg)
    ppg_r = resample_to(ppg_f)
    ppg_n = ppg_r - ppg_r.min()
    rng = float(ppg_n.max() - ppg_n.min())
    if rng <= 1e-9:
        raise ValueError('PPG 是直线（手指没放 / 传感器没出光）')
    ppg_n = ppg_n / rng

    x = ppg_n.reshape(1, TARGET_LENGTH, 1)
    approx = m['mdl1'].predict(x, verbose=0)
    if isinstance(approx, list):
        approx = approx[0]
    refined = m['mdl2'].predict(approx, verbose=0)
    if isinstance(refined, list):
        refined = refined[0]

    lo, hi = m['meta']['min_abp'], m['meta']['max_abp']
    abp = refined.ravel() * (hi - lo) + lo
    return float(abp.max()), float(abp.min()), float(abp.mean()), abp


# ---------------------------------------------------------------------------
# 数据获取
# ---------------------------------------------------------------------------

def read_csv_ppg(path: str):
    """读页面导出的 CSV（t_ms,ecg_raw,ppg_ir），返回 100 Hz 均匀网格上的 PPG。"""
    d = np.loadtxt(path, delimiter=',', skiprows=1)
    t = d[:, 0] / 1000.0
    ppg = d[:, 2]
    if t[-1] <= t[0]:
        return ppg
    grid = np.arange(t[0], t[-1], 0.01)
    return np.interp(grid, t, ppg)


def capture_serial(port: str, baud: int, need: int, timeout: float = 30.0):
    """从串口读 0xBB 帧的 IR，凑够 need 个样本。"""
    import serial
    LEN = {0xAA: 16, 0xBB: 17, 0xCC: 5}
    ser = serial.Serial(port, baud, timeout=0.5)
    print(f'[ppg2abp] 采集中：{port} @ {baud}，需要 {need} 个 PPG 样本（约 {need/100:.1f} s）')
    out, buf, t0 = [], b'', time.time()
    try:
        while len(out) < need:
            if time.time() - t0 > timeout:
                raise TimeoutError(f'超时：只收到 {len(out)}/{need} 个 PPG 样本')
            buf += ser.read(max(1, ser.in_waiting))
            i = 0
            while i < len(buf):
                L = LEN.get(buf[i])
                if L is None:
                    i += 1
                    continue
                if len(buf) - i < L:
                    break
                pkt = buf[i:i + L]
                c = 0
                for x in pkt[:L - 1]:
                    c ^= x
                if c != pkt[L - 1]:
                    i += 1
                    continue
                if buf[i] == 0xBB:
                    out.append(struct.unpack('>I', pkt[4:8])[0])
                i += L
            buf = buf[i:]
    finally:
        ser.close()
    print(f'[ppg2abp] 收到 {len(out)} 个样本')
    return np.array(out, dtype=np.float64)


def predict_bp_avg(ppg: np.ndarray, window: int = RAW_LENGTH, k: int = 3,
                   model_dir: str | None = None):
    """把信号切成 k 个连续窗口分别推理再平均，降低单窗抖动。

    原版 bp_web_v3 用 3 次滑动平均（BUFFER_SIZE=3），这里等价实现。
    返回 (sbp, dbp, map, 各窗明细, 代表波形)。
    """
    ppg = np.asarray(ppg, dtype=np.float64)
    usable = (len(ppg) // window) * window
    if usable < window:
        raise ValueError(f'样本不足一个窗口：{len(ppg)} < {window}')
    seg = ppg[len(ppg) - usable:]
    n = usable // window
    k = max(1, min(k, n))

    rows, waves = [], []
    for i in range(k):
        part = seg[i * window:(i + 1) * window]
        sbp, dbp, mp, abp = predict_bp(part, model_dir)
        rows.append((sbp, dbp, mp))
        waves.append(abp)

    arr = np.array(rows)
    return (float(arr[:, 0].mean()), float(arr[:, 1].mean()), float(arr[:, 2].mean()),
            rows, waves[-1])


def main():
    ap = argparse.ArgumentParser(description='PPG2ABP：PPG → ABP → 血压')
    ap.add_argument('--csv', help='离线：页面导出的 CSV')
    ap.add_argument('--port', help='在线：串口，例如 COM9')
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--window', type=float, default=8.2, help='分析窗口（秒），默认 8.2')
    ap.add_argument('--model-dir', help='PPG2ABP 工程目录（含 models/ 与 models.py）')
    ap.add_argument('--avg', type=int, default=3, help='多窗平均次数，默认 3（与原版一致）')
    args = ap.parse_args()

    win = max(RAW_LENGTH, int(round(args.window * SENSOR_RATE)))
    need = win * max(1, args.avg)

    if args.csv:
        ppg = read_csv_ppg(args.csv)
        print(f'[ppg2abp] CSV 载入：{len(ppg)} 点 @100Hz（{len(ppg)/100:.1f} s）')
        if len(ppg) < need:
            raise SystemExit(f'CSV 太短：{len(ppg)} < {need}')
        seg = ppg
    elif args.port:
        seg = capture_serial(args.port, args.baud, need)
    else:
        ap.error('需要 --csv 或 --port')

    sbp, dbp, mapv, rows, abp = predict_bp_avg(seg, win, args.avg, args.model_dir)
    if len(rows) > 1:
        print(f'[ppg2abp] {len(rows)} 窗平均：' +
              ' | '.join('%.1f/%.1f/%.1f' % r for r in rows))
    print()
    print('=' * 46)
    print(f'  SBP = {sbp:6.1f} mmHg')
    print(f'  DBP = {dbp:6.1f} mmHg')
    print(f'  MAP = {mapv:6.1f} mmHg')
    print(f'  ABP 波形：{abp.shape[0]} 点，范围 {abp.min():.1f} ~ {abp.max():.1f}')
    print('=' * 46)
    ok = 80 <= sbp <= 200 and 40 <= dbp <= 120 and sbp - dbp >= 15
    print('生理合理性：', 'PASS' if ok else 'FAIL（检查 PPG 是否稳定、手指是否压实）')


if __name__ == '__main__':
    main()
