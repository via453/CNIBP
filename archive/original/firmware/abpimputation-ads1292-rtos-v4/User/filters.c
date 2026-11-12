// /*****************************************************************
//   Copyright (c) [2026], [EUIN_模电课设] All rights reserved
//   File Name   : filters.c
//   Description : 生理信号数字滤波器模块实现文件，包含移动平均滤波器和IIR滤波器的
//                 初始化和应用函数实现
//   History     :
//       euin   [2026/03/19]   create
//       euin   [2026/03/19]   [新增功能] 添加IIR滤波器实现
//       euin   [2026/03/19]   [优化功能] 添加高通滤波器以减少基线漂移
// *****************************************************************/
 
// /* ================================= 头文件包含区 ================================= */
// #include "filters.h"

// /* ================================= 私有宏定义区 ================================= */

// /* ================================= 私有类型定义区 ================================= */

// /* ================================= 私有全局变量区 ================================= */
// // 陷波滤波器状态变量
// static int32_t g_notch_x1 = 0, g_notch_x2 = 0;
// static int32_t g_notch_y1 = 0, g_notch_y2 = 0;
// // 低通滤波器状态变量
// static int32_t g_lowpass_y1 = 0;
// /* ================================= 私有函数声明区 ================================= */

// /* ================================= 私有函数实现区 ================================= */

// /* ================================= 导出函数实现区 ================================= */

// /**
//  * @brief  初始化移动平均滤波器
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @retval None
//  */
// void SPO2_FILTER_MovAvgInit(SPO2_MovAvgFilter_t *SPO2_filter)
// {
//     memset(SPO2_filter, 0, sizeof(SPO2_MovAvgFilter_t));
// }

// /**
//  * @brief  应用移动平均滤波算法
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval uint16_t 滤波后的输出数据
//  */
// uint16_t SPO2_FILTER_MovAvgApply(SPO2_MovAvgFilter_t *SPO2_filter, uint16_t SPO2_input)
// {
//     SPO2_filter->SPO2_sum -= SPO2_filter->SPO2_buf[SPO2_filter->SPO2_index];
//     SPO2_filter->SPO2_buf[SPO2_filter->SPO2_index] = SPO2_input;
//     SPO2_filter->SPO2_sum += SPO2_input;

//     SPO2_filter->SPO2_index++;
//     if (SPO2_filter->SPO2_index >= SPO2_MOVAVG_WIN)
//         SPO2_filter->SPO2_index = 0;

//     if (SPO2_filter->SPO2_count < SPO2_MOVAVG_WIN)
//         SPO2_filter->SPO2_count++;

//     return (uint16_t)(SPO2_filter->SPO2_sum / SPO2_filter->SPO2_count);
// }

// /**
//  * @brief  初始化IIR滤波器
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_alpha: 滤波系数 (0 < alpha < 1)
//  * @retval None
//  * @note   alpha越小平滑程度越高，alpha越大响应越快
//  */
// void SPO2_FILTER_IIRInit(SPO2_IIRFilter_t *SPO2_filter, float SPO2_alpha)
// {
//     SPO2_filter->SPO2_alpha = SPO2_alpha;
//     SPO2_filter->SPO2_prev_output = 0.0f;
//     SPO2_filter->SPO2_initialized = 0;
// }

// /**
//  * @brief  应用IIR滤波算法
//  * @param  SPO2_filter: 滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval float 滤波后的输出数据
//  */
// float SPO2_FILTER_IIRApply(SPO2_IIRFilter_t *SPO2_filter, float SPO2_input)
// {
//     float SPO2_output;
    
//     if (!SPO2_filter->SPO2_initialized)
//     {
//         // 第一次调用时，直接返回输入值作为初始值
//         SPO2_output = SPO2_input;
//         SPO2_filter->SPO2_prev_output = SPO2_output;
//         SPO2_filter->SPO2_initialized = 1;
//     }
//     else
//     {
//         // IIR滤波公式: y(n) = alpha * x(n) + (1 - alpha) * y(n-1)
//         SPO2_output = SPO2_filter->SPO2_alpha * SPO2_input + 
//                      (1.0f - SPO2_filter->SPO2_alpha) * SPO2_filter->SPO2_prev_output;
//         SPO2_filter->SPO2_prev_output = SPO2_output;
//     }
    
//     return SPO2_output;
// }

// /**
//  * @brief  初始化高通滤波器
//  * @param  SPO2_filter: 高通滤波器结构体指针
//  * @param  SPO2_coeff: 滤波系数，值越小截止频率越低
//  * @retval None
//  */
// void SPO2_FILTER_HPInit(SPO2_HPFilter_t *SPO2_filter, float SPO2_coeff)
// {
//     SPO2_filter->SPO2_lp_prev_input = 0.0f;
//     SPO2_filter->SPO2_lp_prev_output = 0.0f;
//     SPO2_filter->SPO2_hp_coeff = SPO2_coeff;
//     SPO2_filter->SPO2_initialized = 0;
// }

// /**
//  * @brief  应用高通滤波算法
//  * @param  SPO2_filter: 高通滤波器结构体指针
//  * @param  SPO2_input: 输入的待滤波数据
//  * @retval float 滤波后的输出数据
//  */
// float SPO2_FILTER_HPApply(SPO2_HPFilter_t *SPO2_filter, float SPO2_input)
// {
//     float SPO2_output;
//     float SPO2_lp_output;
    
//     if (!SPO2_filter->SPO2_initialized)
//     {
//         // 第一次调用时，初始化状态
//         SPO2_filter->SPO2_lp_prev_input = SPO2_input;
//         SPO2_filter->SPO2_lp_prev_output = SPO2_input;
//         SPO2_filter->SPO2_initialized = 1;
        
//         return 0.0f;  // 初始时刻返回0，避免突变
//     }
    
//     // 实现一阶高通滤波器
//     // 高通 = 原始信号 - 低通信号
//     // 低通滤波: y_low(n) = coeff * x(n) + (1-coeff) * y_low(n-1)
//     SPO2_lp_output = SPO2_filter->SPO2_hp_coeff * SPO2_input + 
//                      (1.0f - SPO2_filter->SPO2_hp_coeff) * SPO2_filter->SPO2_lp_prev_output;
    
//     // 高通输出 = 原始输入 - 低通输出
//     SPO2_output = SPO2_input - SPO2_lp_output;
    
//     // 更新状态
//     SPO2_filter->SPO2_lp_prev_input = SPO2_input;
//     SPO2_filter->SPO2_lp_prev_output = SPO2_lp_output;
    
//     return SPO2_output;
// }


// /**
//  * @brief  单导联数据工频陷波滤波
//  * @note   双二阶陷波滤波器（50/60Hz），采样率250Hz时参数优化
//  * @param  data: 输入输出数据（滤波后覆盖原数据）
//  * @retval None
//  */
// void ECG_Notch_Filter(int32_t* data)
// {
// #if ECG_FILTER_ENABLE
//     // 50Hz陷波滤波器参数（采样率250Hz）
//     const int32_t a0 = 1024;
//     const int32_t a1 = -1448;
//     const int32_t a2 = 1024;
//     const int32_t b1 = 1448;
//     const int32_t b2 = -794;
    
//     int32_t x0 = *data;
//     int32_t y0;
    
//     // 滤波计算（定点数实现，避免浮点运算）
//     y0 = (a0 * x0 + a1 * g_notch_x1 + a2 * g_notch_x2 - b1 * g_notch_y1 - b2 * g_notch_y2) / 1024;
    
//     // 更新状态
//     g_notch_x2 = g_notch_x1;
//     g_notch_x1 = x0;
//     g_notch_y2 = g_notch_y1;
//     g_notch_y1 = y0;
    
//     // 输出滤波后数据
//     *data = y0;
// #endif
// }

// /**
//  * @brief  单导联数据低通滤波
//  * @note   一阶RC低通滤波，截止频率100Hz（采样率250Hz）
//  * @param  data: 输入输出数据（滤波后覆盖原数据）
//  * @retval None
//  */
// void ECG_Lowpass_Filter(int32_t* data)
// {
// #if ECG_FILTER_ENABLE
//     // 一阶低通滤波系数：alpha=0.2
//     const int32_t alpha = 205;  // 0.2*1024
//     int32_t x0 = *data;
//     int32_t y0;
    
//     // 滤波计算：y0 = alpha*x0 + (1-alpha)*y1
//     y0 = (alpha * x0 + (1024 - alpha) * g_lowpass_y1) / 1024;
    
//     // 更新状态
//     g_lowpass_y1 = y0;
    
//     // 输出滤波后数据
//     *data = y0;
// #endif
// }

// /**
//  * @brief  12导联数据整体滤波
//  * @note   依次对每个导联进行陷波+低通滤波
//  * @param  ecg_data: 12导联数据（输入输出）
//  * @retval None
//  */
// void ECG_Filter_12Lead_Data(ECG_12Lead_Data_t* ecg_data)
// {
// #if ECG_FILTER_ENABLE
//     if (ecg_data == NULL)
//     {
//         return;
//     }
    
//     // 对每个导联依次滤波
//     ECG_Notch_Filter(&ecg_data->Lead_I);
//     ECG_Lowpass_Filter(&ecg_data->Lead_I);
    
//     ECG_Notch_Filter(&ecg_data->Lead_II);
//     ECG_Lowpass_Filter(&ecg_data->Lead_II);
    
//     ECG_Notch_Filter(&ecg_data->Lead_III);
//     ECG_Lowpass_Filter(&ecg_data->Lead_III);
    
//     ECG_Notch_Filter(&ecg_data->Lead_aVR);
//     ECG_Lowpass_Filter(&ecg_data->Lead_aVR);
    
//     ECG_Notch_Filter(&ecg_data->Lead_aVL);
//     ECG_Lowpass_Filter(&ecg_data->Lead_aVL);
    
//     ECG_Notch_Filter(&ecg_data->Lead_aVF);
//     ECG_Lowpass_Filter(&ecg_data->Lead_aVF);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V1);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V1);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V2);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V2);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V3);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V3);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V4);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V4);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V5);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V5);
    
//     ECG_Notch_Filter(&ecg_data->Lead_V6);
//     ECG_Lowpass_Filter(&ecg_data->Lead_V6);
// #endif
// }
