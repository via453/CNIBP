/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : Delay.c
  Description : 延时函数实现，基于 SysTick 定时器提供精确延时
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

/* ================================ 头文件包含区 ================================ */

#include "stm32f10x.h"                  // STM32F10x 设备头文件
#include "Delay.h"                      // 延时函数声明


/* ================================ 私有宏定义区 ================================ */

/* 本文件无特殊宏定义 */


/* ================================ 私有类型定义区 ============================== */

/* 本文件无自定义类型定义 */


/* ================================ 私有全局变量区 ============================== */

/* 本文件无私有全局变量 */


/* ================================ 私有函数声明区 ============================== */

/* 本文件无私有函数声明 */


/* ================================ 导出函数实现区 ============================== */

/**
 * @brief  微秒级延时函数
 * @param  xus 延时时长，单位：微秒
 * @retval 无
 * @note   v1 修复：原实现直接改写 SysTick->CTRL 并在结束时写 0x00000004，
 *         等于把 SysTick 关掉，FreeRTOS 的系统节拍就此停止（所有任务永久阻塞，
 *         只剩 idle 任务在跑）。这里改用 Cortex-M3 的 DWT 周期计数器做忙等，
 *         完全不碰 SysTick，任务态/中断态都能安全调用。
 */
/* v1: 用 Cortex-M3 的 DWT 周期计数器做延时，避开 SysTick 与 FreeRTOS 的冲突。
   这里直接写寄存器地址，避免不同 CMSIS 版本 DWT_Type / CoreDebug 定义差异。 */
#define DEMCR_ADDR      (0xE000EDFCUL)   /* Debug Exception and Monitor Control */
#define DEMCR_TRCENA    (1UL << 24)
#define DWT_CTRL_ADDR   (0xE0001000UL)   /* DWT Control */
#define DWT_CYCCNT_ADDR (0xE0001004UL)   /* DWT Cycle Count */
#define DWT_CYCCNTENA   (1UL << 0)

void Delay_us(uint32_t xus)
{
    volatile uint32_t *demcr  = (volatile uint32_t *)DEMCR_ADDR;
    volatile uint32_t *dwtc   = (volatile uint32_t *)DWT_CTRL_ADDR;
    volatile uint32_t *cycnt  = (volatile uint32_t *)DWT_CYCCNT_ADDR;
    uint32_t start, ticks;

    if ((*demcr & DEMCR_TRCENA) == 0)
    {
        *demcr |= DEMCR_TRCENA;      /* 使能跟踪单元 */
        *cycnt  = 0;
        *dwtc  |= DWT_CYCCNTENA;     /* 启动周期计数器 */
    }

    ticks = xus * (SystemCoreClock / 1000000U);
    start = *cycnt;
    while ((*cycnt - start) < ticks)
    {
        /* 忙等；无符号减法天然处理计数器回绕 */
    }
}

/**
 * @brief  毫秒级延时函数
 * @param  xms 延时时长，单位：毫秒
 * @retval 无
 * @note   阻塞式忙等。任务中若需让出 CPU 请用 vTaskDelay()。
 */
void Delay_ms(uint32_t xms)
{
    while (xms--)
    {
        Delay_us(1000);
    }
}

/**
 * @brief  秒级延时函数
 * @param  xs 延时时长，单位：秒
 * @retval 无
 */
void Delay_s(uint32_t xs)
{
    while (xs--)
    {
        Delay_ms(1000);
    }
}

