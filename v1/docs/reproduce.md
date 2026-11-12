# 复现步骤

## 0. 环境

- Python 3.8/3.9（TF 2.6 不支持 3.13）
- `pip install -r algorithm/abpimputation/requirements.txt`
- 需要 `pyserial`、`flask`、`scipy`、`numpy<1.24`

## 1. 无硬件：算法自检

```bash
cd v1/algorithm/abpimputation
python test_synthetic_inference.py
```

预期：`ALL CHECKS PASSED`，SBP 80–200、DBP 40–150。

## 2. 无硬件：桥接自检

```bash
python abp_sensor_bridge.py --selftest
```

预期：`SBP 100–140 / DBP 60–90 / MAP 75–105`，波形形状 `(3200,)`。

## 3. 无硬件：网页模拟模式

```bash
cd ../../web/realtime_denoise
python main.py
```

打开 `http://localhost:8080`。约 32 秒后控制台出现 `[模型] cNIBP: SBP=… DBP=… MAP=…`。

> 模拟模式的 cNIBP 若显示 `92/60/71`，那是 `main.py` 的固定演示值，不代表模型输出；以第 1、2 步为准。

## 4. 有硬件：协议体检

```bash
python v1/tools/probe_serial.py --port COM37 --duration 30
```

预期：ECG 248–252/s、PPG 98–102/s、TEMP ≈1/s、bad=0。

## 5. 有硬件：端到端

```bash
python v1/web/realtime_denoise/main.py --port COM37 --nibp-sys 120 --nibp-dias 80
```

32 秒后出 cNIBP；SBP 90–140 / DBP 55–90 / MAP 70–105；与袖带读数偏差 <15/10 mmHg。

## 常见问题

| 现象 | 原因 | 处理 |
|:--|:--|:--|
| PPG 帧率 ~250/s | 未烧 v1 固件 | 重新编译烧录 |
| bad 计数高 | 波特率/接线/共地 | 检查 CH340 与 GND |
| cNIBP 不更新 | 窗口不足 32s | 等待或检查 `window_cover_sec` |
| 导入 `signal_processing` 失败 | `_ABP_ROOT` 未指向 `v1/algorithm` | 确认目录结构 |
