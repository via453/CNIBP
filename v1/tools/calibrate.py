# -*- coding: utf-8 -*-
"""多点个体校准工具。

用法::

    python C:\\Users\\Zelvia\\cnibp.staging\\v1\\tools\\calibrate.py ^
        --point "PPG.csv=100/66" ^
        --point "104_71.csv=104/71" ^
        --point "100_62.csv=100/62"

每个 ``--point`` 是「CSV 路径=袖带读数」。CSV 用页面导出的格式
``t_ms,ecg_raw,ppg_ir``（只用到 ppg_ir）。

处理流程：
1. PPG 去重 → 线性插值到 100 Hz
2. 自动裁掉首尾的手指脱离段（IR 低于中位数 50% 的连续段）
3. 按 8.2 秒切窗，逐窗跑 PPG2ABP，取均值作为一个测量点
4. 多点拟合校准（偏置 + 脉压缩放），保存到 algorithm/calib.json
5. 打印线性回归诊断，明确告知斜率是否可用

为什么要打印诊断：本受试者多个测量点的参考血压跨度很小（SBP 4 / DBP 9 mmHg），
小于模型自身窗间抖动。此时线性拟合得到的斜率不可信，强行使用会在
外推到其他血压水平时严重失真。工具会把斜率/r² 算出来并判定是否可用。
"""

import argparse
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ALGO = os.path.abspath(os.path.join(HERE, '..', 'algorithm'))
sys.path.insert(0, ALGO)

from bp_calibration import BPCalibration, parse_pair, DEFAULT_CALIB_PATH  # noqa: E402
from ppg2abp_bridge import predict_bp, RAW_LENGTH  # noqa: E402

FS_PPG = 100


def load_ppg(path):
    """读取页面导出的 CSV，返回 100 Hz 等间隔 PPG。"""
    d = np.loadtxt(path, delimiter=',', skiprows=1)
    if d.ndim == 1:
        d = d.reshape(1, -1)
    t = d[:, 0] / 1000.0
    ppg = d[:, 2]

    # PPG 每 2.5 行（250Hz/100Hz）才更新一次，去重后按时间插值
    uniq_t, uniq_v = [], []
    last = None
    for tt, vv in zip(t, ppg):
        if vv != last:
            uniq_t.append(tt)
            uniq_v.append(vv)
            last = vv
    uniq_t = np.asarray(uniq_t)
    uniq_v = np.asarray(uniq_v)

    n = int((uniq_t[-1] - uniq_t[0]) * FS_PPG)
    grid = np.linspace(uniq_t[0], uniq_t[-1], num=n)
    return np.interp(grid, uniq_t, uniq_v)


def trim_dropout(p, ratio=0.5):
    """裁掉首尾手指脱离段，返回 (最长连续有效段, 起止秒)。"""
    med = np.median(p)
    ok = p > ratio * med
    best = (0, 0)
    s = None
    for i, v in enumerate(ok):
        if v and s is None:
            s = i
        elif not v and s is not None:
            if i - s > best[1] - best[0]:
                best = (s, i)
            s = None
    if s is not None and len(ok) - s > best[1] - best[0]:
        best = (s, len(ok))
    a, b = best
    return p[a:b], (a / FS_PPG, b / FS_PPG)


def measure_point(path):
    """跑一个 CSV，返回 (sbp, dbp, 窗明细, 稳定段描述)。"""
    raw = load_ppg(path)
    seg, (t0, t1) = trim_dropout(raw)
    n = len(seg) // RAW_LENGTH
    if n < 1:
        raise SystemExit(f'{path}: 稳定段只有 {len(seg)} 点，不足一个 8.2s 窗口')

    rows = []
    for i in range(n):
        part = seg[i * RAW_LENGTH:(i + 1) * RAW_LENGTH]
        s, d, m, _ = predict_bp(part)
        rows.append((s, d, m))
    arr = np.array(rows)
    return (float(arr[:, 0].mean()), float(arr[:, 1].mean()),
            rows, (t0, t1, len(seg), n))


def main():
    ap = argparse.ArgumentParser(description='PPG2ABP 多点个体校准')
    ap.add_argument('--point', action='append', required=True,
                    metavar='CSV=SBP/DBP',
                    help='一个测量点，例如 "PPG.csv=100/66"，可重复')
    ap.add_argument('--out', default=None, help='校准输出路径，默认 algorithm/calib.json')
    ap.add_argument('--points-out', default=None,
                    help='把原始测量点另存为 JSON（便于以后追加）')
    args = ap.parse_args()

    cases = []
    print('=' * 68)
    print('逐点测量')
    print('=' * 68)
    for spec in args.point:
        if '=' not in spec:
            raise SystemExit(f'--point 格式应为 CSV=SBP/DBP，收到：{spec}')
        path, ref = spec.rsplit('=', 1)
        rs, rd = parse_pair(ref)
        s, d, rows, info = measure_point(path)
        t0, t1, npts, nwin = info
        print()
        print('%s' % os.path.basename(path))
        print('  参考 %d/%d   稳定段 %.1f~%.1f s（%d 点）→ %d 窗'
              % (rs, rd, t0, t1, npts, nwin))
        for i, (a, b, _c) in enumerate(rows):
            print('    窗%d: %6.1f / %5.1f' % (i + 1, a, b))
        sd_s = float(np.std([r[0] for r in rows]))
        sd_d = float(np.std([r[1] for r in rows]))
        print('  均值 %6.1f / %5.1f  (窗间 sd %.1f / %.1f)'
              % (s, d, sd_s, sd_d))
        print('  偏差 %+6.1f / %+5.1f' % (s - rs, d - rd))

        cases.append({
            'file': os.path.basename(path),
            'sbp_ref': rs, 'dbp_ref': rd,
            'sbp_model': round(s, 2), 'dbp_model': round(d, 2),
            'sbp_sd': round(sd_s, 2), 'dbp_sd': round(sd_d, 2),
            'n_windows': nwin,
        })

    # ---- 拟合 ----
    cal = BPCalibration.from_points(cases, source=','.join(c['file'] for c in cases))
    print()
    print('=' * 68)
    print('校准结果')
    print('=' * 68)
    print('  点数 %d' % len(cases))
    print('  MAP 偏移 %+.2f mmHg     脉压 ×%.4f' % (cal.offset_map, cal.scale_pp))

    # ---- 回代 ----
    print()
    print('%-14s %-9s %-11s %-15s %-11s' % ('文件', '参考', '模型', '校准后', '误差'))
    errs = []
    for c in cases:
        s, d = c['sbp_model'], c['dbp_model']
        cs, cd, _ = cal.apply_triplet(s, d, (s + 2 * d) / 3)
        es, ed = cs - c['sbp_ref'], cd - c['dbp_ref']
        errs.append((es, ed))
        print('%-14s %-9s %-11s %6.1f/%-8.1f %+5.1f/%+5.1f'
              % (c['file'], '%d/%d' % (c['sbp_ref'], c['dbp_ref']),
                 '%.1f/%.1f' % (s, d), cs, cd, es, ed))
    e = np.array(errs)
    print('  MAE: SBP %.2f  DBP %.2f     最大: SBP %.2f  DBP %.2f'
          % (np.abs(e[:, 0]).mean(), np.abs(e[:, 1]).mean(),
             np.abs(e[:, 0]).max(), np.abs(e[:, 1]).max()))

    # ---- 斜率诊断 ----
    print()
    print('=' * 68)
    print('线性诊断（能否做斜率标定）')
    print('=' * 68)
    dg = cal.diagnostics
    for key, name in (('sbp', 'SBP'), ('dbp', 'DBP')):
        s = dg.get(key) or {}
        if s.get('slope') is None:
            print('  %s: 参考值无变化，无法拟合' % name)
            continue
        print('  %s: 斜率 %.3f  r² %.3f   参考跨度 %.0f  模型跨度 %.0f'
              % (name, s['slope'], s['r2'] if s['r2'] is not None else float('nan'),
                 s['span_ref'], s['span_model']))
    usable = dg.get('linear_sbp_usable') and dg.get('linear_dbp_usable')
    if usable:
        print('  → 斜率可用，可考虑线性标定')
    else:
        print('  → 斜率不可用：参考变化幅度接近模型自身抖动，')
        print('     已按「偏置 + 脉压缩放」校准，不做线性外推。')
        print('     要拿到可信斜率，需要在更大血压范围内取点（例如运动前后各测一次）。')

    # ---- 保存 ----
    out = args.out or DEFAULT_CALIB_PATH
    cal.save(out)
    print()
    print('校准已保存：%s' % out)
    if args.points_out:
        with open(args.points_out, 'w', encoding='utf-8') as f:
            json.dump(cases, f, ensure_ascii=False, indent=2)
        print('测量点已保存：%s' % args.points_out)
    print()
    print('提示：以上是「回代误差」（同一批点既用于拟合又用于检验），')
    print('      不代表准确度。真正的验证要另取一次测量作独立检验。')


if __name__ == '__main__':
    main()
