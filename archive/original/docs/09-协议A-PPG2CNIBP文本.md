# 协议 A：PPG2CNIBP（逐行文本，100Hz）

对应固件：`stm32f103c8t6_MAX30102_PRO` @ `TJC`
对应上位机：`fork_BP/Projects/PPG2ABP/codes/bp_web_v3.py`

## 物理层

- USART1，115200 bps，8N1
- 仅发送 PPG 单通道（RED），不发 IR，无帧头、无长度、无校验

## 帧格式

ASCII 文本，每行一个整数，行尾 `\r\n`：

```
12345\r\n
12346\r\n
12344\r\n
...
```

发送点：`User/blood.c:82`

```c
printf("%.0f\r\n", filtered_red);
```

## 采样率与时序

- MAX30102 配置 100Hz（`REG_SPO2_CONFIG = 0x2A`）
- 每个样本发送一行，无时间戳；上位机按到达顺序计数

## 上位机处理

`bp_web_v3.py`：

1. `read_ppg_raw()` 连续读 820 行，`float(line)`
2. 0.5–5Hz 3 阶 Butterworth 带通 + `filtfilt`
3. `scipy.signal.resample` 到 1024 点（等效 125Hz）
4. min-max 归一化到 [0,1]
5. `UNetDS64`（`ApproximateNetwork.h5`）→ `MultiResUNet1D`（`RefinementNetwork.h5`）
6. 反归一化（`meta9.p`：min_abp 50.0 / max_abp ≈199.99）
7. SBP=max、DBP=min、MAP=mean；3 点滑动平均 + 校准偏移

## 注意

- 无校验，丢一行不会报错，只会让时间轴轻微偏移
- 上位机按「行」而不是「字节」同步，串口噪声中出现非数字行会被 `except ValueError: continue` 跳过
