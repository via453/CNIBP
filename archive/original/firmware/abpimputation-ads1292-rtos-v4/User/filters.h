// /*****************************************************************
//   Copyright (c) [2026], [EUIN_模电课设] All rights reserved
//   File Name   : filters.h
//   Description : 生理信号数字滤波器模块头文件，包含移动平均滤波器和IIR滤波器的数据结构定义
//                 和相关函数接口声明
//   History     :
//       euin   [2026/03/19]   create
//       euin   [2026/03/19]   [新增功能] 添加IIR滤波器实现
//       euin   [2026/03/19]   [优化功能] 添加高通滤波器以减少基线漂移
// *****************************************************************/



// /* ================================= 头文件保护宏 ================================= */
// #ifndef __FILTERS_H
// #define __FILTERS_H

// /* ================================= 依赖头文件包含区 ================================= */
// #include "stm32f10x.h"
// // #include "spo2_config.h"
// #include "ECG_CONFIG.h"
// /* ================================= 导出宏定义区 ================================= */
// #define ECG_FILTER_ENABLE      1       // 0:禁用滤波，1:启用滤波
// #define ECG_NOTCH_FREQ         50      // 工频陷波频率（50/60Hz）
// #define ECG_LOWPASS_FREQ       100     // 低通滤波截止频率（Hz）
// /* ================================= 导出类型定义区 ================================= */
// /**
//  * @brief SPO2移动平均滤波器结构体
//  */
// typedef struct
// {
//     uint16_t SPO2_buf[SPO2_MOVAVG_WIN];                             /*!< 滤波器缓冲区 */
//     uint8_t  SPO2_index;                                           /*!< 当前缓冲区索引 */
//     uint8_t  SPO2_count;                                           /*!< 当前缓冲区有效元素数量 */
//     uint32_t SPO2_sum;                                             /*!< 缓冲区中所有元素的和 */
// } SPO2_MovAvgFilter_t;

// /**
//  * @brief SPO2 IIR滤波器结构体
//  * @note  一阶低通IIR滤波器: y(n) = alpha*x(n) + (1-alpha)*y(n-1)
//  */
// typedef struct
// {
//     float    SPO2_alpha;                                           /*!< 滤波系数 */
//     float    SPO2_prev_output;                                     /*!< 上一次的输出值 */
//     uint8_t  SPO2_initialized;                                     /*!< 是否已初始化标志 */
// } SPO2_IIRFilter_t;

// /**
//  * @brief SPO2高通滤波器结构体
//  * @note  用于去除直流偏移和低频漂移
//  */
// typedef struct
// {
//     float    SPO2_lp_prev_input;                                   /*!< 上一次低通输入 */
//     float    SPO2_lp_prev_output;                                  /*!< 上一次低通输出 */
//     float    SPO2_hp_coeff;                                        /*!< 高通滤波系数 */
//     uint8_t  SPO2_initialized;                                     /*!< 是否已初始化标志 */
// } SPO2_HPFilter_t;

// /* ================================= 导出常量定义区 ================================= */

// /* ================================= 导出全局变量声明区 ================================= */

// /* ================================= 导出函数声明区 ================================= */
// /**
//  * @brief  初始化移动平均滤波器
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @retval None
//  */
// void SPO2_FILTER_MovAvgInit(SPO2_MovAvgFilter_t *SPO2_filter);

// /**
//  * @brief  应用移动平均滤波算法
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval uint16_t 滤波后的输出数据
//  */
// uint16_t SPO2_FILTER_MovAvgApply(SPO2_MovAvgFilter_t *SPO2_filter, uint16_t SPO2_input);

// /**
//  * @brief  初始化IIR滤波器
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_alpha: 滤波系数 (0 < alpha < 1)
//  * @retval None
//  * @note   alpha越小平滑程度越高，alpha越大响应越快
//  */
// void SPO2_FILTER_IIRInit(SPO2_IIRFilter_t *SPO2_filter, float SPO2_alpha);

// /**
//  * @brief  应用IIR滤波算法
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval float 滤波后的输出数据
//  */
// float SPO2_FILTER_IIRApply(SPO2_IIRFilter_t *SPO2_filter, float SPO2_input);

// /**
//  * @brief  初始化高通滤波器
//  * @param  SPO2_filter: 高通滤波器结构体指针
//  * @param  SPO2_coeff: 滤波系数，值越小截止频率越低
//  * @retval None
//  */
// void SPO2_FILTER_HPInit(SPO2_HPFilter_t *SPO2_filter, float SPO2_coeff);

// /**
//  * @brief  应用高通滤波算法
//  * @param  SPO2_filter: 高通滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval float 滤波后的输出数据
//  */
// float SPO2_FILTER_HPApply(SPO2_HPFilter_t *SPO2_filter, float SPO2_input);

// /**
//  * @brief  单导联数据工频陷波滤波
//  * @note   双二阶陷波滤波器（50/60Hz）
//  * @param  data: 输入输出数据（滤波后覆盖原数据）
//  * @retval None
//  */
// void ECG_Notch_Filter(int32_t* data);

// /**
//  * @brief  单导联数据低通滤波
//  * @note   一阶RC低通滤波
//  * @param  data: 输入输出数据（滤波后覆盖原数据）
//  * @retval None
//  */
// void ECG_Lowpass_Filter(int32_t* data);

// /**
//  * @brief  12导联数据整体滤波
//  * @note   依次对每个导联进行陷波+低通滤波
//  * @param  ecg_data: 12导联数据（输入输出）
//  * @retval None
//  */
// void ECG_Filter_12Lead_Data(ECG_12Lead_Data_t* ecg_data);


// #endif /* __FILTERS_H */