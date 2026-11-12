#include "tjc.h"
#include <stdio.h>

/**
 * @brief  发送TJC命令（自动追加0xFF 0xFF 0xFF终止符）
 * @param  cmd: 命令字符串（不含终止符）
 * @retval None
 */
void TJC_SendCmd(char *cmd)
{
    while (*cmd)
    {
        Usart_SendByte(TJC_USART, (uint8_t)*cmd);
        cmd++;
    }
    /* 发送TJC协议终止符: 3个0xFF */
    Usart_SendByte(TJC_USART, TJC_END_1);
    Usart_SendByte(TJC_USART, TJC_END_2);
    Usart_SendByte(TJC_USART, TJC_END_3);
}

/**
 * @brief  发送波形数据点到TJC串口屏
 * @param  channel: 波形通道号 (0, 1, 2, ...)
 * @param  value:   波形数据值 (0-255)
 * @retval None
 * @note   命令格式: "add s{id}.id,0,{value}" + 0xFF x3
 */
void TJC_SetWaveform(uint8_t channel, uint8_t value)
{
    char buf[32];
    sprintf(buf, "add s%d.id,0,%d", channel, value);
    TJC_SendCmd(buf);
}

/**
 * @brief  设置TJC串口屏数值控件
 * @param  component: 控件名称，如 "n0", "n1"
 * @param  value:     要设置的数值
 * @retval None
 * @note   命令格式: "{component}.val={value}" + 0xFF x3
 */
void TJC_SetNumber(char *component, int32_t value)
{
    char buf[40];
    sprintf(buf, "%s.val=%ld", component, value);
    TJC_SendCmd(buf);
}

/**
 * @brief  PPG波形归一化：将原始PPG值映射到0-255范围
 * @param  raw_value: MAX30102原始PPG值 (uint16_t)
 * @retval 归一化后的值 (0-255)
 * @note   使用100样本滑动窗口动态追踪min/max
 *         - raw_value==0（传感器未连接）时返回0
 *         - min==max（无变化）时返回128
 *         - 纯整数运算，无浮点数
 */
#define PPG_WINDOW_SIZE  100

uint8_t TJC_NormalizePPG(uint16_t raw_value)
{
    static uint16_t window[PPG_WINDOW_SIZE] = {0};
    static uint8_t  win_index     = 0;
    static uint8_t  sample_count  = 0;

    /* 传感器未连接或断开 */
    if (raw_value == 0)
        return 0;

    /* 写入环形缓冲区 */
    window[win_index] = raw_value;
    win_index++;
    if (win_index >= PPG_WINDOW_SIZE)
        win_index = 0;
    if (sample_count < PPG_WINDOW_SIZE)
        sample_count++;

    /* 扫描窗口求min/max */
    uint16_t min_val = window[0];
    uint16_t max_val = window[0];
    uint8_t  i;

    for (i = 1; i < sample_count; i++)
    {
        if (window[i] < min_val) min_val = window[i];
        if (window[i] > max_val) max_val = window[i];
    }

    /* 无变化，返回中点 */
    if (max_val == min_val)
        return 128;

    /* 归一化: (raw - min) * 255 / (max - min) */
    uint32_t range      = (uint32_t)(max_val - min_val);
    uint32_t normalized = ((uint32_t)(raw_value - min_val) * 255) / range;

    return (uint8_t)(normalized > 255 ? 255 : normalized);
}
