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
        }

    @classmethod
    def from_dict(cls, d):
        return cls(d['sbp_ref'], d['dbp_ref'], d['sbp_model'], d['dbp_model'],
                   created=d.get('created'), source=d.get('source', ''))

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

    # ------------------------------------------------------------ 描述
    def describe(self):
        return (f'校准点：模型 {self.sbp_model:.1f}/{self.dbp_model:.1f}'
                f'（脉压 {self.pp_model:.1f}）→ 参考 {self.sbp_ref:.0f}/{self.dbp_ref:.0f}'
                f'（脉压 {self.pp_ref:.1f}）；'
                f'MAP 偏移 {self.offset_map:+.1f} mmHg，脉压 ×{self.scale_pp:.3f}')


def parse_pair(text: str):
    """解析 `100/66` 这类字符串。"""
    parts = str(text).replace('／', '/').split('/')
    if len(parts) != 2:
        raise ValueError(f'参考值格式应为 SBP/DBP，例如 100/66，收到：{text!r}')
    return float(parts[0]), float(parts[1])
