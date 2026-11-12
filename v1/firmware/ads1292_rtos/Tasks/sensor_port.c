/**
  * @file    sensor_port.c
  * @brief   V4 协议与传感器驱动的适配层
  * @note    实现 main.c 中声明的两个 extern 函数：
  *          - ADS1292_ReadECG()  : 从 g_ads1292_cache 取 CH1 并转 uV
  *          - MAX30102_Read()    : 读 1 个 FIFO 样本（IR/RED）
  */

#include "ECG.h"
#include "MAX30102.h"

/* get_volt 在 ECG.c 中实现，ECG.h 未导出声明 */
extern int32_t get_volt(uint32_t num);

/**
  * @brief  读取一次 ECG（微伏）
  * @retval 有符号 32 位微伏值；无新样本时返回上一次值
  * @note   g_ads1292_cache[0..2] 为状态，CH1 数据在 [3..5]（24bit 大端）
  */
int32_t ADS1292_ReadECG(void)
{
    static int32_t last = 0;

    if (g_ads1292_recv_flag)
    {
        uint32_t raw;

        /* 关中断拷贝：DRDY 中断随时可能重写 g_ads1292_cache，
           不保护会出现高低字节来自两帧的撕裂值 */
        __disable_irq();
        raw = ((uint32_t)g_ads1292_cache[3] << 16)
            | ((uint32_t)g_ads1292_cache[4] << 8)
            |  (uint32_t)g_ads1292_cache[5];
        g_ads1292_recv_flag = 0;
        __enable_irq();

        last = get_volt(raw);
    }

    return last;
}

/**
  * @brief  从 ECG 环形缓冲取一个样本
  * @param  out 输出：ECG 微伏值
  * @retval 1=取到，0=当前没有新样本
  * @note   单生产者(ISR)/单消费者(任务)，head/tail 各由一方更新，无需关中断。
  */
int32_t ADS1292_PopECG(int32_t *out)
{
    if (g_ecg_ring_tail == g_ecg_ring_head)
    {
        return 0;
    }

    *out = g_ecg_ring[g_ecg_ring_tail];
    g_ecg_ring_tail = (uint8_t)((g_ecg_ring_tail + 1u) % ECG_RING_SIZE);
    return 1;
}

/**
  * @brief  读取一次 PPG FIFO 样本
  * @param  ir  : 输出 IR 值
  * @param  red : 输出 RED 值
  */
void MAX30102_Read(uint32_t *ir, uint32_t *red)
{
    max30102_read_fifo();

    if (ir  != 0) *ir  = (uint32_t)fifo_ir;
    if (red != 0) *red = (uint32_t)fifo_red;
}
