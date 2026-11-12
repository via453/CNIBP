/**
 * @file    ecg_raw_data.h
 * @brief   ECG原始未滤波数据 (从 kai4.csv 转换, ~250Hz, 5000点)
 * @note    EUIN_模电课设 / PESC Project, 2026
 */
#ifndef __ECG_RAW_DATA_H
#define __ECG_RAW_DATA_H

#include <stdint.h>

#define ECG_RAW_DATA_LEN  5000

extern const float g_ecg_raw_data[ECG_RAW_DATA_LEN];

#endif /* __ECG_RAW_DATA_H */
