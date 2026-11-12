# 协议对照（v1 以 B 为准）

## A. PPG2CNIBP（逐行文本）

- 115200 8N1，每行一个整数 + `\r\n`，100Hz
- 发送点：`stm32f103c8t6_MAX30102_PRO/User/blood.c:82`
- 接收：`bp_web_v3.py` 的 `read_ppg_raw()`（820 行 → 1024 点 @125Hz）

## B. V4 三帧（v1 使用）

| 类型 | 帧头 | 长度 | 字段 | 校验 | 频率 |
|:--|:--|:--|:--|:--|:--|
| ECG | `AA AA` | 16 | `F3 07` + uV(int32 BE) + hr + status + cmrr + ts(uint32 BE) | XOR | 250Hz |
| PPG | `BB BB` | 17 | `F2 0C` + IR(u32) + RED(u32) + ts(u32) | XOR | 100Hz |
| TEMP | `CC CC` | 5 | temp×100(int16) | XOR | 1Hz |

XOR = 前 n-1 字节异或，与末字节比较。

## C. 过渡态 12B ECG

`AA AA F3 07` + uV(int32 BE) + hr + status + cmrr + 累加和，共 12 字节，无 PPG。

## 已知差异

- 12B 与 16B 不兼容：长度、时间戳、校验都不同
- 原固件 PPG 以 250Hz 发（零阶保持重复），v1 已改为 100Hz 累加器
- `abp_sensor_bridge.py` 原按 12B/17B 累加和解析，v1 已改用 V4 的 16B/17B XOR
