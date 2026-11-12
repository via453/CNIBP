# v1 — ABPImputation 路线复现第一版

对应暂定对象：**ABPImputation（V-Net / PAT）推理复刻**。链路：

```
STM32（ADS1292R + MAX30102）
  → USART1 115200：0xAA 16B ECG / 0xBB 17B PPG / 0xCC 5B TEMP（XOR）
  → main.py（Flask + SSE :8080）：32s 窗口 → 9 通道 → ABPImputer V-Net
  → 网页 cNIBP 面板：SBP / DBP / MAP
```

## 目录

```
v1/
├── firmware/ads1292_rtos/     # V4 固件（已补 ECG/MAX30102 驱动与适配层，PPG 改 100Hz）
├── web/realtime_denoise/      # main.py + index.html + data/（kai4.csv、ppg_waveform.csv）
├── algorithm/abpimputation/   # V-Net 推理包 + SavedModel + 桥接脚本
├── tools/probe_serial.py      # 串口帧率/校验体检
└── docs/                      # 协议、数据、复现步骤
```

## 快速开始

### 无硬件（先验证算法与网页）

```bash
cd v1/algorithm/abpimputation
python test_synthetic_inference.py          # 模型自检
python abp_sensor_bridge.py --selftest      # 桥接自检
cd ../../web/realtime_denoise
python main.py                              # http://localhost:8080
```

### 有硬件

```bash
python v1/tools/probe_serial.py --port COM37 --duration 30
python v1/web/realtime_denoise/main.py --port COM37 --nibp-sys 120 --nibp-dias 80
```

## v1 相对原版的改动

| 文件 | 改动 |
|:--|:--|
| `Tasks/ECG.c/.h` | 从过渡态移植；关闭 `ECG_DEMO_MODE`；删除 `ECG_Task` 与 demo 数据依赖 |
| `Tasks/MAX30102.c/.h`、`User/MAX30102_CONFIG.c/.h` | 从过渡态移植 |
| `Tasks/sensor_port.c` | 新增：实现 `ADS1292_ReadECG()` / `MAX30102_Read()` |
| `User/main.c` | `Sensor_Task` 改为 ECG 新样本触发 + PPG 100Hz 累加器 |
| `.eide/eide.yml` | 删除不存在的 `spi.*` / `ads1292r.*`，补 `ECG.*`、`MAX30102.*`、`sensor_port.c` |
| `algorithm/signal_processing.py` | 新增 V4 的 16B/17B/5B 解析与 `parse_frame()` |
| `algorithm/abp_sensor_bridge.py` | `capture_sensor_data()` 改用 V4 解析（read 15 字节）；新增 `--selftest` |
| `web/realtime_denoise/main.py` | 路径相对化；NIBP 参数化；PPG 250→100Hz 兜底降采样 |

未改：模型权重、`abpimputation/models/vnet.py`、`preprocessing/features.py`、V4 帧格式。
