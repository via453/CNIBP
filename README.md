# cnibp — 无袖带连续血压测量（课设原版归档 + 复现）

本仓库保存一套无袖带连续血压测量课设的**原始源码快照**，并在 `main` 分支上给出**复现第一版**。

## 两条技术路线

| | 路线 A：PPG2CNIBP | 路线 B：ABPImputation |
|:--|:--|:--|
| 传感器 | MAX30102（PPG） | ADS1292R（ECG）+ MAX30102（PPG） |
| 固件 | `stm32f103c8t6_MAX30102_PRO` @ `TJC` | `ads1292_rtos` V4 三帧 |
| 串口 | 逐行文本，100Hz | `0xAA` 16B / `0xBB` 17B / `0xCC` 5B，XOR |
| 上位机 | `bp_web_v3.py`（Flask + Socket.IO，:5000） | `main.py`（Flask + SSE，:8080） |
| 算法 | UNetDS64 + MultiResUNet1D | ABPImputation V-Net（PAT） |

## 分支

- **`archive/original`** — 原始源码冻结快照，tag `v0-original`
- **`main`** — 复现第一版（v1），先跑通路线 B（ABPImputation）

## 目录

```
archive/original/           # 原版（仅 archive 分支）
├── firmware/               # 三套固件：TJC / V4 / 过渡态 12B
├── web-ppg2abp/            # 路线 A 上位机 + 两个 .h5（LFS）
├── web-abpimputation/      # 路线 B 上位机（Flask + SSE）
├── algorithm/abpimputation/# V-Net 推理包 + SavedModel
└── docs/                   # 报告正文、数据表、三份协议说明
```

`main` 的 `v1/` 见 `v1/docs/reproduce.md`。

## 数据说明

仓库**不含** 19GB 训练数据（`data.hdf5`、`train*.p`、`val*.p`、`test.p`）。推理只需 `meta9.p`（242 B）与 SavedModel，已在仓库中。完整清单见 `archive/original/README-ARCHIVE.md`。

## 许可

仅供课程设计与学习参考。第三方项目版权归原作者：PPG2ABP（MIT）、ABPImputation（见其 CITATION）。
