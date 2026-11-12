"""诊断模拟窗口：把 main.py 喂给模型的同一份数据单独跑一遍，看每一步是否合理。

用法：
    python v1/tools/diag_sim_window.py

输出：ECG/PPG 统计量、R 波与 PPG 峰个数、HR、PAT、9 通道特征与模型输出。
用于判断「网页上 cNIBP 数值异常」到底是数据问题还是模型问题。
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
WEB = os.path.abspath(os.path.join(HERE, '..', 'web', 'realtime_denoise'))
ALG = os.path.abspath(os.path.join(HERE, '..', 'algorithm'))
sys.path.insert(0, ALG)

from signal_processing import (          # noqa: E402
    resample_ecg, detect_r_peaks, detect_ppg_peaks,
    calculate_pat, calculate_hr_from_ecg,
)

FS = 250
PPG_FS = 100
WIN = 32


def load_sim():
    rec = os.path.join(WEB, 'data', 'sim_record.csv')
    if os.path.exists(rec):
        from scipy import signal as _sig
        d = np.loadtxt(rec, delimiter=',', skiprows=1)
        # 记录 100Hz，界面按 250Hz 播放
        return _sig.resample_poly(d[:, 1], 5, 2), d[:, 2], 'sim_record.csv'
    ecg_raw = np.loadtxt(os.path.join(WEB, 'data', 'kai4.csv'),
                         delimiter=',', skiprows=1)[:, 1]
    ppg = []
    with open(os.path.join(WEB, 'data', 'ppg_waveform.csv'), encoding='utf-8') as f:
        import csv
        for row in csv.DictReader(f):
            ppg.append(float(row['ppg_value']))
    return ecg_raw, np.array(ppg, dtype=np.float64), 'kai4 + ppg_waveform'


def describe(name, x):
    print(f"  {name}: n={len(x)} min={x.min():.3f} max={x.max():.3f} "
          f"mean={x.mean():.3f} std={x.std():.3f}")


def main():
    print("=" * 64)
    print("模拟窗口诊断（与 main.py 模拟模式同源数据）")
    print("=" * 64)

    ecg_raw, ppg_raw, src = load_sim()
    print(f'  数据源: {src}')
    describe('ECG 原始', ecg_raw)
    describe('PPG 原始', ppg_raw)

    # 取 32 秒（循环取样，模拟 main.py 的环形播放）
    n_ecg = WIN * FS
    n_ppg = WIN * PPG_FS
    ecg = np.resize(ecg_raw, n_ecg)
    ppg = np.resize(ppg_raw, n_ppg)

    ecg100 = resample_ecg(ecg, orig_fs=FS, target_fs=PPG_FS)
    describe('ECG@100Hz', ecg100)

    r = detect_r_peaks(ecg100, fs=PPG_FS)
    p = detect_ppg_peaks(ppg, fs=PPG_FS)
    hr = calculate_hr_from_ecg(r, fs=PPG_FS)
    pat_med, pat_std = calculate_pat(r, p, fs=PPG_FS)

    print(f"\n  R 波个数 = {len(r)}   首 5 个位置 = {r[:5]}")
    print(f"  PPG 峰个数 = {len(p)}  首 5 个位置 = {p[:5]}")
    print(f"  HR = {hr:.1f} bpm")
    print(f"  PAT = {pat_med:.1f} ± {pat_std:.1f} ms")

    print("\n  判定：")
    ok = True
    if len(r) < 10:
        print(f"    ✗ R 波过少（{len(r)}），HR 无意义 → 模型输入失真")
        ok = False
    if not (100 <= pat_med <= 400):
        print(f"    ✗ PAT {pat_med:.1f} ms 超出 100–400 ms 生理范围")
        ok = False
    if ok:
        print("    ✓ 峰值与 PAT 均合理，模型输出可用")

    if '--model' in sys.argv:
        from abp_sensor_bridge import process_sensor_data
        sbp, dbp, mp, wf = process_sensor_data(ecg, ppg, 120, 80)
        print(f"\n  模型输出：SBP={sbp:.1f} DBP={dbp:.1f} MAP={mp:.1f} shape={wf.shape}")

    print("=" * 64)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
