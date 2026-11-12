# 原始源码快照（archive/original）

本分支是 2026-11-13 对 `G:\Class_Design` 相关工程的**只读快照**，用于原版保留。所有文件按原样复制，未做代码修改；唯一处理是排除构建产物、`.git`、`.sisyphus` 与含人名的 `.uvguix.*`。

## 来源表

| 归档目录 | 源路径 | 分支 / 版本 | commit | 备注 |
|:--|:--|:--|:--|:--|
| `firmware/ppg2cnibp-max30102-stm32f103` | `G:\Class_Design\Class_Design\stm32f103c8t6_MAX30102_PRO` | `TJC` | `a27c27f` | 2026-06-03「fork_BP配套下位机」；逐行文本 PPG |
| `firmware/abpimputation-ads1292-rtos-v4` | `G:\Class_Design\Class_Design\ads1292_rtos` | `main` | `03fea90` + 147 行未提交改动 | V4 三帧 16/17/5B；读取函数未实现 |
| `firmware/transitional-ecg12b-ads1292-rtos` | `G:\Class_Design\Class_Design\ecg-ppg-all\ads1292_rtos` | `接入py` | `07fb837` + 未提交改动 | 只发 12B ECG，无 PPG |
| `web-ppg2abp` | `G:\Class_Design\Class_Design\fork_BP\Projects\PPG2ABP\codes` | 工作区 | – | 排除 `data/`(18.9GB) 与 `BP.zip`；两个 `.h5` 走 LFS |
| `web-abpimputation` | `G:\Class_Design\realtime_denoise\realtime_denoise` | `backup-v4`（tag `v4`） | `d1aea3d` | 含工作区未提交改动；`.git` 未复制 |
| `algorithm/abpimputation` | `G:\ABPImputation` | `main` | `76f06b4` | 含本地 Windows 兼容改动与 SavedModel |
| `docs` | `G:\Class_Design\章节*.md` 等 | – | – | 报告正文、数据表、答辩稿、素材索引 |

## 体积

| 部分 | 大小 |
|:--|:--|
| 三套固件 | ≈26 MB |
| `web-ppg2abp` 源码 | ≈12 MB |
| `web-ppg2abp` 权重（LFS） | 267 MB（126,057,312 + 141,216,776 B） |
| `web-abpimputation` | 6.2 MB |
| `algorithm/abpimputation` | 31 MB |
| `docs` | ≈0.1 MB |

## 未归档的数据（19GB）

以下文件**不在仓库中**，需要时从原机 `G:\Class_Design\Class_Design\fork_BP\Projects\PPG2ABP\codes\data\` 取回或重新生成：

| 文件 | 大小 | 说明 |
|:--|:--|:--|
| `data.hdf5` | 2,545,202,048 B | 预处理后的训练集 |
| `train0.p` … `train9.p` | 各 1,474,560,244 B | 10 折训练集 |
| `val0.p` … `val9.p` | 各 163,840,236 B | 10 折验证集 |
| `test.p` | 446,628,078 B | 测试集 |
| `meta0.p` … `meta9.p` | 各 242 B | 归一化参数（`meta9.p` 推理必需） |
| `BP.zip` | 247,475,224 B | 原始归档 |

`meta9.p` 只有 242 B，已在 `web-ppg2abp/data/` 中保留一份，推理不需要其余大数据。

## 隐私处理

- 报告 `.docx` / `.pdf` 封面含姓名、学号、指导教师，**未归档**（`.gitignore` 已排除 `*.docx`、`课设报告*`）
- 固件工程里的 `Project.uvguix.*`（含用户名）已删除
- `realtime_denoise/.git` 未复制（其 `config` 内曾含明文 PAT，已提醒吊销）
