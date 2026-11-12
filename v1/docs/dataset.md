# 数据集说明

仓库**不含** 19GB 训练数据。推理只需要 `meta9.p`（242 B）与 SavedModel，二者已在仓库中。

| 文件 | 大小 | 说明 |
|:--|:--|:--|
| `data.hdf5` | 2,545,202,048 B | 预处理训练集 |
| `train0.p` … `train9.p` | 各 1,474,560,244 B | 10 折训练集 |
| `val0.p` … `val9.p` | 各 163,840,236 B | 10 折验证集 |
| `test.p` | 446,628,078 B | 测试集 |
| `meta0.p` … `meta9.p` | 各 242 B | 归一化参数 |
| `BP.zip` | 247,475,224 B | 原始归档 |

## 获取方式

1. 从原机 `G:\Class_Design\Class_Design\fork_BP\Projects\PPG2ABP\codes\data\` 复制；
2. 或按原项目 `data_processing.py` → `data_handling.py` 从原始 MIMIC/UCI 数据重新生成（耗时长，需 GPU/大内存）。

## 推理所需最小集

`v1/algorithm/abpimputation/abpimputation/models/vnet_32s_mimic/`（saved_model.pb + variables + 6 个 pkl）与 `v1/web/realtime_denoise/data/` 下的 `kai4.csv`、`ppg_waveform.csv`。
