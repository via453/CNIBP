# 复现步骤

## 0. 环境（已在原作机器上实测通过）

| 项 | 实测值 |
|:--|:--|
| conda 根 | `D:\microsoft_download\anaconda3\Anaconda` |
| 环境名 | `abpimputation` |
| Python | 3.8.20 |
| TensorFlow | 2.6.0（tensorflow-cpu） |
| numpy | **1.21.6** |
| pandas | 1.5.3 |
| scipy / scikit-learn | 1.10.1 / 1.3.2 |

激活命令（Windows cmd / Anaconda Prompt）：

```bat
conda activate abpimputation
python -c "import tensorflow as tf; print(tf.__version__)"   :: 2.6.0
```

### 为什么必须 numpy 1.21.6

原 `requirements.txt` 写的是 `pandas==1.1.3`，但本机环境装的是 pandas 1.5.3，
而 pandas 1.5.3 要求 `numpy>=1.20.3`；环境里却是 1.19.5 → `import pandas` 直接 ImportError：

```
ImportError: this version of pandas is incompatible with numpy < 1.20.3
your numpy version is 1.19.5.
```

处理方式（一次即可）：

```bat
pip install "numpy==1.21.6"
```

TF 2.6.0 的元数据写着 `numpy~=1.19.2`，pip 会提示 conflict，但实测 1.21.6 下
`tensorflow / scipy / sklearn / h5py / matplotlib` 全部正常，模型推理结果与原始验收记录一致。
**不要**升到 numpy ≥1.24，TF 2.6 会因 `np.object` 等移除而报错。

其余依赖：

```bat
pip install -r algorithm/requirements.txt
```

## 1. 无硬件：算法自检

```bat
cd v1\algorithm
python test_synthetic_inference.py
```

实测输出：

```
Total params: 2,180,321
5. Results:
   Prediction shape: (1, 3200)
   SBP (max): 123.8 mmHg
   DBP (min): 71.8 mmHg
   Mean ABP: 82.5 mmHg
6. Plausibility checks: 5/5 PASS
ALL CHECKS PASSED
```

与原始计划 T5 的证据（123.8 / 71.8 / 82.5）逐位一致。

## 2. 无硬件：桥接自检

```bat
cd v1\algorithm
python abp_sensor_bridge.py --selftest
```

实测输出：

```
[selftest] SBP=121.3 DBP=75.0 MAP=101.2 shape=(3200,)
```

（原始计划 T7b 记录 120.5 / 75.2 / 101.1，差异来自合成数据随机种子，量级一致。）

## 3. 无硬件：网页模拟模式

```bat
cd v1\web\realtime_denoise
python main.py
```

打开 `http://localhost:8080`。实测：HTTP 200（25,089 B），页面含「连续血压 cNIBP」
SBP/DBP/MAP 面板；`/stream` 每 20ms 推一帧 JSON 波形。约 32 秒后控制台出现
`[模型] cNIBP: SBP=… DBP=… MAP=…`。

> 模拟模式的 cNIBP 若显示 `92/60/71`，那是 `main.py` 的固定演示值，不代表模型输出；以第 1、2 步为准。

## 4. 有硬件：编译烧录（v1 固件）

工程：`v1/firmware/ads1292_rtos`（EIDE，AC5 / Cortex-M3 / STM32F103C8，IROM 64K、IRAM 20K）。

1. EIDE 打开该目录 → 选 `Target 1` → Build。
2. ST-Link SWD 烧录（`0x08000000`）。
3. 相对归档版，v1 的编译相关改动：
   - `Tasks/ECG.c/.h`、`Tasks/MAX30102.c/.h`、`User/MAX30102_CONFIG.c/.h` 新增进工程（`.eide/eide.yml` 已改）；
   - 删掉 `eide.yml` 里从未存在的 `Tasks/ads1292r.c`、`Tasks/spi.c/.h`；
   - `ECG.h` 注释掉 `ECG_DEMO_MODE`，`ECG.c` 删掉 `ECG_Task` 与 demo 数据 include；
   - `ADS1292_PowerOnInit()` 里两处裸 `printf` 加上 `#ifdef DEBUG_ADS1292`（否则 ASCII 会混入 115200 的二进制帧流）。
4. 串口助手 @115200 应看到 `AA AA F3 07…` / `BB BB F2 0C…` / `CC CC…` 三类帧。

## 5. 有硬件：协议体检

```bat
python v1\tools\probe_serial.py --port COM37 --duration 30
```

预期：ECG 248–252/s、PPG 98–102/s、TEMP ≈1/s、bad=0。

## 6. 有硬件：端到端

```bat
python v1\web\realtime_denoise\main.py --port COM37 --nibp-sys 120 --nibp-dias 80
```

32 秒后出 cNIBP；SBP 90–140 / DBP 55–90 / MAP 70–105；与袖带读数偏差 <15/10 mmHg。

## 常见问题

| 现象 | 原因 | 处理 |
|:--|:--|:--|
| PPG 帧率 ~250/s | 未烧 v1 固件 | 重新编译烧录 |
| bad 计数高 | 波特率/接线/共地 | 检查 CH340 与 GND |
| cNIBP 不更新 | 窗口不足 32s | 等待或检查 `window_cover_sec` |
| 导入 `signal_processing` 失败 | `_ABP_ROOT` 未指向 `v1/algorithm` | 确认目录结构 |
