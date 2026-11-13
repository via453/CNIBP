# -*- coding: utf-8 -*-
"""PPG2ABP 的个体校准。

背景
----
PPG2ABP 的权重在 MIMIC（ICU 人群）上训练，那批受试者脉压普遍较宽。
直接推理会出现系统性偏差，本项目实测（同一受试者、60 秒 7 窗）：

    模型 SBP 139.5 ± 10.3   DBP 62.0 ± 3.2   脉压 77.5
    袖带 SBP 100            DBP 66          脉压 34

注意偏差方向：SBP 高估 +39.5，DBP 反而低估 −4.0，脉压宽了一倍多。
这种「一边高一边低」的形态说明它是**尺度问题**，只用加偏移修不好：
加偏移能把 SBP/DBP 拉到参考值，但脉压仍是 77.5，下一次测量就会自相矛盾。

做法
----
仿射校准，两步：

1) MAP 平移：把模型平均压整体对齐到参考平均压；
2) 脉压缩放：把模型脉压按「参考脉压 / 模型脉压」的比例压缩。

两者合起来是一个仿射变换，在标定点上能精确复现参考值，
同时保留模型逐次变化的相对幅度（变化量按同一比例缩放）。

参数只用两处常量（MAP 偏移、脉压比例），不会过拟合单点噪声。
"""

import json
import os
import time

# 标准 MAP 近似：(SBP + 2·DBP) / 3
def _map_of(sbp: float, dbp: float) -> float:
    return (sbp + 2.0 * dbp) / 3.0


DEFAULT_CALIB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'calib.json')


class BPCalibration:
    """两点（SBP/DBP）单次测量拟合出的仿射校准。"""

    def __init__(self, sbp_ref, dbp_ref, sbp_model, dbp_model,
                 created=None, source=''):
        # 参考侧
        self.sbp_ref = float(sbp_ref)
        self.dbp_ref = float(dbp_ref)
        self.map_ref = _map_of(self.sbp_ref, self.dbp_ref)
        self.pp_ref = self.sbp_ref - self.dbp_ref

        # 模型侧（同一时刻的原始输出）
        self.sbp_model = float(sbp_model)
        self.dbp_model = float(dbp_model)
        self.map_model = _map_of(self.sbp_model, self.dbp_model)
        self.pp_model = self.sbp_model - self.dbp_model

        # 两个校准量
        self.offset_map = self.map_ref - self.map_model
        self.scale_pp = (self.pp_ref / self.pp_model) if self.pp_model > 1e-6 else 1.0

        self.created = created or time.strftime('%Y-%m-%dT%H:%M:%S')
        self.source = source

        # 多点拟合时保留原始点与诊断（单点拟合为空）
        self.points = []
        self.diagnostics = {}

    # ------------------------------------------------------------------ 应用
    def apply(self, sbp: float, dbp: float):
        """把模型输出映射到校准后的血压。"""
        m = _map_of(sbp, dbp) + self.offset_map
        pp = (sbp - dbp) * self.scale_pp
        return m + pp * 2.0 / 3.0, m - pp / 3.0

    def apply_triplet(self, sbp: float, dbp: float, mapv: float):
        """同时返回校准后的 MAP（用校准后 SBP/DBP 重算，保证三者自洽）。"""
        s, d = self.apply(sbp, dbp)
        return s, d, _map_of(s, d)

    def clamp(self, sbp: float, dbp: float, lo_s=60.0, hi_s=220.0,
              lo_d=30.0, hi_d=140.0):
        """越界判定：校准后仍不合理的直接判为无效。"""
        return not (lo_s <= sbp <= hi_s and lo_d <= dbp <= hi_d and sbp - dbp >= 10)

    # ------------------------------------------------------------ 序列化
    def to_dict(self):
        return {
            'sbp_ref': self.sbp_ref, 'dbp_ref': self.dbp_ref,
            'sbp_model': self.sbp_model, 'dbp_model': self.dbp_model,
            'offset_map': round(self.offset_map, 3),
            'scale_pp': round(self.scale_pp, 4),
            'pp_ref': round(self.pp_ref, 1),
            'pp_model': round(self.pp_model, 1),
            'created': self.created,
            'source': self.source,
            'n_points': len(self.points) if self.points else 1,
            'points': self.points,
            'diagnostics': self.diagnostics,
        }

    @classmethod
    def from_dict(cls, d):
        obj = cls(d['sbp_ref'], d['dbp_ref'], d['sbp_model'], d['dbp_model'],
                  created=d.get('created'), source=d.get('source', ''))
        obj.points = d.get('points', []) or []
        obj.diagnostics = d.get('diagnostics', {}) or {}
        return obj

    def save(self, path=None):
        path = path or DEFAULT_CALIB_PATH
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, ensure_ascii=False, indent=2)
        return path

    @classmethod
    def load(cls, path=None):
        path = path or DEFAULT_CALIB_PATH
        if not os.path.exists(path):
            return None
        try:
            with open(path, encoding='utf-8') as f:
                return cls.from_dict(json.load(f))
        except Exception:
            return None

    # -------------------------------------------------------- 多点拟合
    @classmethod
    def from_points(cls, points, source='multi-point'):
        """用多个独立测量点拟合校准。

        points: [{'sbp_ref','dbp_ref','sbp_model','dbp_model'}, ...]

        只拟合两个量（MAP 偏移、脉压比例），全部取均值，不做线性回归。
        原因：本受试者 3 个测量点的参考血压跨度只有 SBP 4 / DBP 9 mmHg，
        小于模型自身的窗间抖动（SBP sd 6~16），线性拟合得到
        SBP 斜率 0.40（r² 0.25）、DBP 斜率 0.007（r² 0.007），
        即模型对真实变化几乎没有分辨力。这种情况下强行拟合斜率
        会把噪声当信号，外推到其他血压水平会严重失真，
        因此只用偏置 + 脉压缩放，并把斜率诊断存下来供审阅。
        """
        if not points:
            raise ValueError('至少需要一个测量点')

        def _map(s, d):
            return _map_of(s, d)

        deltas = [_map(p['sbp_ref'], p['dbp_ref']) - _map(p['sbp_model'], p['dbp_model'])
                  for p in points]
        pp_refs = [p['sbp_ref'] - p['dbp_ref'] for p in points]
        pp_mods = [p['sbp_model'] - p['dbp_model'] for p in points]

        offset_map = sum(deltas) / len(deltas)
        mean_pp_ref = sum(pp_refs) / len(pp_refs)
        mean_pp_mod = sum(pp_mods) / len(pp_mods)
        scale_pp = (mean_pp_ref / mean_pp_mod) if abs(mean_pp_mod) > 1e-6 else 1.0

        # 用「均值点」作为基准点，便于 describe() 与单点版本保持一致
        sbp_m = sum(p['sbp_model'] for p in points) / len(points)
        dbp_m = sum(p['dbp_model'] for p in points) / len(points)
        sbp_r = sum(p['sbp_ref'] for p in points) / len(points)
        dbp_r = sum(p['dbp_ref'] for p in points) / len(points)

        obj = cls(sbp_r, dbp_r, sbp_m, dbp_m, source=source)
        # 用多点统计值覆盖（均值点的 MAP 偏移与 pp 比例可能略有出入）
        obj.offset_map = offset_map
        obj.scale_pp = scale_pp
        obj.points = [dict(p) for p in points]
        obj.diagnostics = cls._linear_diagnostics(points)
        return obj

    @staticmethod
    def _linear_diagnostics(points):
        """计算线性回归诊断，仅用于评估「能不能做斜率标定」。"""
        import math

        def _fit(xs, ys):
            n = len(xs)
            if n < 2:
                return None
            mx = sum(xs) / n
            my = sum(ys) / n
            sxx = sum((x - mx) ** 2 for x in xs)
            if sxx < 1e-12:
                return {'slope': None, 'r2': None, 'span_ref': 0.0, 'span_model': 0.0}
            sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
            slope = sxy / sxx
            intercept = my - slope * mx
            ss_res = sum((y - (slope * x + intercept)) ** 2 for x, y in zip(xs, ys))
            ss_tot = sum((y - my) ** 2 for y in ys)
            r2 = 1 - ss_res / ss_tot if ss_tot > 1e-12 else float('nan')
            return {'slope': round(slope, 4),
                    'intercept': round(intercept, 2),
                    'r2': round(r2, 4) if r2 == r2 else None,
                    'span_ref': max(xs) - min(xs),
                    'span_model': max(ys) - min(ys)}

        sbp = _fit([p['sbp_ref'] for p in points],
                   [p['sbp_model'] for p in points])
        dbp = _fit([p['dbp_ref'] for p in points],
                   [p['dbp_model'] for p in points])
        out = {'n_points': len(points), 'sbp': sbp, 'dbp': dbp}
        # 可信度判定：斜率接近 1 且 r² 足够高才认为能做斜率标定
        def _usable(s):
            return (s and s.get('slope') is not None and s.get('r2') is not None
                    and 0.7 <= s['slope'] <= 1.4 and s['r2'] >= 0.7)
        out['linear_sbp_usable'] = bool(_usable(sbp))
        out['linear_dbp_usable'] = bool(_usable(dbp))
        return out

    # ------------------------------------------------------------ 描述
    def describe(self):
        head = (f'校准点（{len(self.points)} 点均值）' if self.points else '校准点')
        txt = (f'{head}：模型 {self.sbp_model:.1f}/{self.dbp_model:.1f}'
               f'（脉压 {self.pp_model:.1f}）→ 参考 {self.sbp_ref:.0f}/{self.dbp_ref:.0f}'
               f'（脉压 {self.pp_ref:.1f}）；'
               f'MAP 偏移 {self.offset_map:+.1f} mmHg，脉压 ×{self.scale_pp:.3f}')
        d = self.diagnostics
        if d:
            sb, db = d.get('sbp') or {}, d.get('dbp') or {}
            txt += (f'\n  线性诊断：SBP 斜率 {sb.get("slope")} (r² {sb.get("r2")})，'
                    f'DBP 斜率 {db.get("slope")} (r² {db.get("r2")})')
            if not (d.get('linear_sbp_usable') and d.get('linear_dbp_usable')):
                txt += '\n  → 斜率不可用，已按「偏置 + 脉压缩放」校准（不做线性外推）'
        return txt


def parse_pair(text: str):
    """解析 `100/66` 这类字符串。"""
    parts = str(text).replace('／', '/').split('/')
    if len(parts) != 2:
        raise ValueError(f'参考值格式应为 SBP/DBP，例如 100/66，收到：{text!r}')
    return float(parts[0]), float(parts[1])
