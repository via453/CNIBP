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

## 3. 无硬件：网页模拟模式（即可看到真实模型数值）

```bat
cd v1\web\realtime_denoise
python main.py
```

打开 `http://localhost:8080`。约 32 秒后控制台出现：

```
[bridge] HR = 72.1 bpm | PAT = 220.0 ± 17.7 ms
[模型] cNIBP: SBP=123.6 DBP=71.4 MAP=80.5
```

面板同时显示 SBP 123.6 / DBP 71.4 / MAP 80.5 与 ABP 波形（`/stream` 的 `cNIBP` 字段）。

### 模拟数据源（v1 修正）

| 文件 | 内容 | 说明 |
|:--|:--|:--|
| `data/sim_record.csv` | `t, ecg, ppg, abp_ref_mmHg`，3200 点 @100Hz（32 s） | **首选**。取自 `algorithm/data/synthetic_test.csv.gz` 的同一受试者记录，ECG/PPG 同源，PAT 有生理意义；参考 ABP 92.6–128.0 mmHg |
| `data/kai4.csv` + `data/ppg_waveform.csv` | 旧数据 | 回退用。二者来自**不同受试者**，算出的 PAT 无意义，会把模型推成越界值（实测出现过 SBP 206 / DBP −223） |

启动日志会打印实际使用的数据源：

```
[模拟] 加载ECG: 8000点 @250Hz
[模拟] 加载PPG: 3200点 @100Hz
[模拟] 数据源: sim_record.csv（ECG/PPG 同一受试者）
```

> v1 另外加了**生理合理性闸门**：`SBP∉[80,200]` 或 `DBP∉[40,120]` 或 `SBP−DBP<15` 的推理结果
> 不写入面板，只在控制台打印 `[模型] 输出越界…已丢弃`，避免把 −223 mmHg 这类值显示给用户。
> 34 秒前尚未跑出模型结果时，面板显示的是演示值 92/60/71。

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

### 没有串口也能验证：SWD 探针

板子只接了 ST-Link、没有 USB 串口时，用 SWD 直接读固件里的计数器：

```bat
D:\microsoft_download\anaconda3\Anaconda\python.exe C:\Users\Zelvia\cnibp.staging\v1\tools\probe_swd.py
```

实测输出（2026-11-13，STM32F103 高密度 256K）：

```
时间基准：tick 差值 10.004 s（1 kHz）
链路               差值       速率/s       期望/s 判定
ecg            2502      250.1        250 PASS
ppg            1001      100.1        100 PASS
temp             10        1.0          1 PASS
主循环速率   : 250.2 /s
初始化阶段号 : 9  （9 = 已进入主循环）
fifo_ir/red  : 11532 / 10941
ALL PASS
```

`probe_swd.py` 依赖固件里的 `g_dbg_*` 计数器（v1 已内置，占 20 字节 RAM），
符号地址从 `build/PhysioCard_v1.axf` 实时解析。

### 一键构建 + 烧录

```bat
bash C:\Users\Zelvia\cnibp.staging\v1\tools\build_firmware.sh
D:\microsoft_download\anaconda3\Anaconda\python.exe -m pyocd flash -t stm32f103rc ^
     C:\Users\Zelvia\cnibp.staging\v1\firmware\ads1292_rtos\build\PhysioCard_v1.hex
```

构建脚本用 armcc/armlink/fromelf（`D:\keil5\ARM\ARMCC\bin`）复刻 EIDE 的参数，
源文件清单取自 `build/Target 1/builder.params`。

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
| cNIBP 一直是 92/60/71 | 未满 32 秒窗口，或推理被闸门丢弃 | 等 35 s；看控制台有没有 `[模型] 输出越界` |
| cNIBP 出现 206 / −223 这类值 | 用的是旧模拟源（ECG/PPG 不同受试者） | 确认 `data/sim_record.csv` 存在；已加闸门不会上屏 |
| HR = 0 bpm、PAT = 250.0 | R 波一个都没检出 | 跑 `python v1/tools/diag_sim_window.py` 定位 |
| 导入 `signal_processing` 失败 | `_ABP_ROOT` 未指向 `v1/algorithm` | 确认目录结构 |
| `import pandas` 报 numpy < 1.20.3 | 环境 numpy 太旧 | `pip install "numpy==1.21.6"` |

## 附：单窗口诊断工具

```bat
python v1\tools\diag_sim_window.py          :: 只看信号链路
python v1\tools\diag_sim_window.py --model  :: 连模型一起跑
```

输出 ECG/PPG 统计、R 波与 PPG 峰个数、HR、PAT 与判定。正常应看到：

```
  R 波个数 = 39   PPG 峰个数 = 39
  HR = 72.1 bpm
  PAT = 220.0 ± 17.7 ms
    ✓ 峰值与 PAT 均合理，模型输出可用
```

## 7. 备选路线：PPG2ABP（只要 PPG，不需要 ECG）

干电极心电质量不达标时用这条。模型：UNetDS64(ApproximateNetwork.h5, 10.5M 参数)
→ MultiResUNet1D(RefinementNetwork.h5, 11.7M 参数)，输入 820 点 @100Hz（8.2 s）。

```bat
:: 实时（读 0xBB 帧的 IR，3 窗平均）
python C://Users//Zelvia//cnibp.staging//v1//algorithm//ppg2abp_bridge.py --port COM9 --avg 3

:: 离线（页面导出的 CSV）
python C://Users//Zelvia//cnibp.staging//v1//algorithm//ppg2abp_bridge.py ^
       --csv "C://Users//Zelvia//DeskBox//NOW_11//课设无袖带血压//调试csv//xxx.csv"
```

模型默认从 `G://Class_Design//Class_Design//fork_BP//Projects//PPG2ABP//codes` 加载（255 MB，未入 Git），
可用 `--model-dir` 或环境变量 `PPG2ABP_HOME` 覆盖。

实测（2026-11-13，同一硬件，手指压在 MAX30102 上）：
- 实时 3 窗：148.2 / 143.7 / 147.0 → 平均 **146.3 / 65.8 / 96.2 mmHg**
- 离线 CSV：**124.0 / 57.6 / 84.7 mmHg**
两次差异来自 PPG 的基线漂移（手在动），模型本身稳定。绝对准确度需袖带读数做参照。

### 7.1 个体校准（必做）

PPG2ABP 的权重在 MIMIC（ICU 人群）上训练，那批人脉压普遍较宽，直接推理会有
**系统性偏差**。本项目实测（同一受试者、60 秒切 7 窗）：

| | 模型原始 | 袖带参考 | 偏差 |
|:--|:--|:--|:--|
| SBP | 139.5 ± 10.3 | 100 | +39.5 |
| DBP | 62.0 ± 3.2 | 66 | −4.0 |
| 脉压 | 77.5 | 34 | +43.5 |

注意偏差形态是「SBP 高估、DBP 反而低估」——说明是尺度问题，不是单纯偏移。
因此校准用**仿射变换**（`v1/algorithm/bp_calibration.py`）而不是加常数：

1. MAP 平移：把模型平均压对齐到参考平均压
2. 脉压缩放：把模型脉压按 `参考脉压 / 模型脉压` 压缩

这样在校准点上能精确复现参考值，同时保留模型逐次变化的相对幅度。

拟合（会写入 `v1/algorithm/calib.json`，已 gitignore）：

```bat
python C://Users//Zelvia//cnibp.staging//v1//algorithm//ppg2abp_bridge.py ^
       --csv PPG.csv --avg 3 --calib 100/66
```

之后每次运行自动加载校准，输出形如：

```
  模型原始输出：SBP 139.6 / DBP 59.9 / MAP 85.8
  ------------------------------------------
  SBP =  100.0 mmHg        DBP =   66.0 mmHg        MAP =   77.3 mmHg
```

其他参数：`--no-calib` 忽略校准只看原始值，`--calib-file` 指定其他校准文件。

**校准后实测（7 窗回代）**：均值 100.7 / 67.6（参考 100/66），脉压 33.0（参考 34），
SBP 跨度 23.3、DBP 跨度 11.0 —— 偏差消除且动态保留。

> 这是**回代验证**，只证明变换正确、动态没被抹平，不代表准确度。
> 真正的准确度要另测一次：隔一段时间、重新袖带测量、再跑一遍比对。
> 单点校准无法预测个体在血压变化时的斜率，需要 2 个以上不同血压点的参考值
> 才能拟合成真正的线性关系。

### 7.2 网页走 PPG2ABP

```bat
python C://Users//Zelvia//cnibp.staging//v1//web//realtime_denoise//main.py ^
       --port COM9 --model ppg2abp
```

打开 `http://localhost:8080`，约 25 秒（8.2 s × 3 窗）后 cNIBP 面板出校准后数值，
同时推送 1024 点 ABP 波形。`--model abpimputation`（默认）仍走 ECG+PPG 的 V-Net 路线。

`--no-calib` 可跳过校准看模型原始输出；`--calib-file` 指定其他校准文件。

实测（2026-11-13，同一受试者）：

```
[校准] 已加载：模型 139.6/59.9（脉压 79.7）→ 参考 100/66（脉压 34.0）；MAP 偏移 -9.2 mmHg，脉压 ×0.427
[模型] PPG2ABP 原始 138.6/62.7/88.2 → 校准后 100.4/68.0/78.8
[模型] PPG2ABP 原始 140.9/62.5/88.5 → 校准后 101.8/68.3/79.5
[模型] PPG2ABP 原始 150.6/65.8/95.0 → 校准后 109.1/72.9/84.9
```

前两次与袖带参考 100/66 相差 0.4/2.0 与 1.8/2.3 mmHg；第三次升高来自手指压力变化。
