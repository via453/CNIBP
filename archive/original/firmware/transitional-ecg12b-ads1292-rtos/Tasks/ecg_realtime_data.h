/**
 * @file    ecg_realtime_data.h
 * @brief   ECG真实采集数据 (从 kai4_filtered_method9.csv 转换, ~250Hz, 5000点)
 * @note    EUIN_模电课设 / PESC Project, 2026
 */
#ifndef __ECG_REALTIME_DATA_H
#define __ECG_REALTIME_DATA_H

#include <stdint.h>

#define ECG_REALTIME_DATA_LEN  5000

extern const float g_ecg_realtime_data[ECG_REALTIME_DATA_LEN];

#endif /* __ECG_REALTIME_DATA_H */
