/**
 * @file    ecg_demo_data.h
 * @brief   ECG演示数据声明 (从 wave.csv 转换, 250 SPS, 2000点)
 * @note    EUIN_模电课设 / PESC Project, 2026
 */

#ifndef __ECG_DEMO_DATA_H
#define __ECG_DEMO_DATA_H

#include <stdint.h>

/* 演示数据长度 */
#define ECG_DEMO_DATA_LEN  2000

/* ECG演示数据数组 (Flash中存储) */
extern const float g_ecg_demo_data[ECG_DEMO_DATA_LEN];

#endif /* __ECG_DEMO_DATA_H */
