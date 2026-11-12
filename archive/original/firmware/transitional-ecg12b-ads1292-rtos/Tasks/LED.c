/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : LED.c
  Description : LED 驱动程序，提供 LED 初始化和控制功能
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

/* ================================ 头文件包含区 ================================ */

#include "stm32f10x.h"                  // STM32F10x 设备头文件
#include "FreeRTOS.h"                   // FreeRTOS 核心头文件
#include "task.h"                       // FreeRTOS 任务管理头文件


/* ================================ 私有宏定义区 ================================ */

/* 本文件无特殊宏定义 */


/* ================================ 私有类型定义区 ============================== */

/* 本文件无自定义类型定义 */


/* ================================ 私有全局变量区 ============================== */

/* 本文件无私有全局变量 */


/* ================================ 私有函数声明区 ============================== */

/* 本文件无私有函数声明 */


/* ================================ 私有函数实现区 ============================== */

/* 本文件无私有函数实现 */


/* ================================ 导出函数实现区 ============================== */

/**
 * @brief  LED 初始化函数
 *         配置 GPIOA(Pin1, Pin2) 和 GPIOC(Pin13) 为推挽输出模式
 * @param  无
 * @retval 无
 */
void LED_Init(void)
{
    /* 开启 GPIO 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   // 开启 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);   // 开启 GPIOC 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // 开启 GPIOB 时钟

    /* GPIO 初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 配置 PA1 和 PA2 为推挽输出，速度 50MHz
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置 PC13 为推挽输出，速度 50MHz
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 配置 PB12 为推挽输出，速度 50MHz
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 设置 GPIO 默认电平（高电平，LED 熄灭） */
    GPIO_SetBits(GPIOA, GPIO_Pin_1 | GPIO_Pin_2);       // 设置 PA1、PA2 为高电平
    GPIO_SetBits(GPIOC, GPIO_Pin_13);                   // 设置 PC13 为高电平
    GPIO_SetBits(GPIOB, GPIO_Pin_12);                    // 设置 PB12 为高电平
}

/**
 * @brief  LED1 开启函数
 *         将 LED1 对应的 GPIO 引脚设置为低电平
 * @param  无
 * @retval 无
 */
void LED1_ON(void)
{
    // GPIO_ResetBits(GPIOA, GPIO_Pin_1);      // 设置 PA1 为低电平
    // GPIO_ResetBits(GPIOC, GPIO_Pin_13);     // 设置 PC13 为低电平
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);      // 设置 PB12 为低电平
}

/**
 * @brief  LED1 关闭函数
 *         将 LED1 对应的 GPIO 引脚设置为高电平
 * @param  无
 * @retval 无
 */
void LED1_OFF(void)
{
    // GPIO_SetBits(GPIOA, GPIO_Pin_1);        // 设置 PA1 为高电平
    // GPIO_SetBits(GPIOC, GPIO_Pin_13);       // 设置 PC13 为高电平
    GPIO_SetBits(GPIOB, GPIO_Pin_12);       // 设置 PB12 为高电平
}

/**
 * @brief  LED1 状态翻转函数
 *         读取当前 LED1 状态并翻转
 * @param  无
 * @retval 无
 */
void LED1_Turn(void)
{
    // 翻转 PA1 引脚状态
    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_8) == 0)     // 当前为低电平
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_8);                    // 设置为高电平
    }
    else                                                    // 当前为高电平
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);                  // 设置为低电平
    }
    
    // 翻转 PC13 引脚状态
    if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) == 0)    // 当前为低电平
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_13);                   // 设置为高电平
    }
    else                                                    // 当前为高电平
    {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);                 // 设置为低电平
    }
}

/**
 * @brief  LED2 开启函数
 *         将 LED2 对应的 GPIO 引脚设置为低电平
 * @param  无
 * @retval 无
 */
void LED2_ON(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_2);      // 设置 PA2 为低电平
}

/**
 * @brief  LED2 关闭函数
 *         将 LED2 对应的 GPIO 引脚设置为高电平
 * @param  无
 * @retval 无
 */
void LED2_OFF(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_2);        // 设置 PA2 为高电平
}

/**
 * @brief  LED2 状态翻转函数
 *         读取当前 LED2 状态并翻转
 * @param  无
 * @retval 无
 */
void LED2_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_2) == 0)     // 当前为低电平
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_2);                    // 设置为高电平
    }
    else                                                    // 当前为高电平
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_2);                  // 设置为低电平
    }
}

/**
 * @brief  LED0 任务函数
 *         周期性翻转 LED1 状态（闪烁）
 * @param  pvParameters: 任务参数（未使用）
 * @retval 无
 */
void led0_task(void *pvParameters)
{
    while (1)
    {
        LED1_Turn();            // 翻转 LED1 状态
        vTaskDelay(200);        // 延时 1200 个系统节拍
    }
}

/**
 * @brief  LED1 任务函数
 *         控制 LED2 以固定频率闪烁
 * @param  pvParameters: 任务参数（未使用）
 * @retval 无
 */
void led1_task(void *pvParameters)
{
    while (1)
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_12);       // 设置 PB12 为高电平
        vTaskDelay(200);        // 延时 1200 个系统节拍
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);     // 熄灭 LED2
        vTaskDelay(200);        // 延时 1200 个系统节拍
    }
}
