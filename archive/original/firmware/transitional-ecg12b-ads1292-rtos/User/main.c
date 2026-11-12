/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : main.c
  Description : 基于 FreeRTOS 的 LED 控制主程序
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

/* ================================ 头文件包含区 ================================ */

#include "tasks_config.h"
#include "stm32f10x.h"
#include "LED.h" 
#include "usart.h"
#include "TEMP.h"
#include "MAX30102_CONFIG.h"
#include "ECG.h"
/* ================================ 全局变量定义区 ============================== */




/* ================================ 函数声明区 ================================ */

static void SystemClock_Init(void);         ///< 系统时钟初始化
static void start_task(void *pvParameters); ///< 开始任务函数
static void Sys_Init(void);                 ///< 系统初始化函数

/* ================================ 函数实现区 ================================ */

/**
 * @brief  系统时钟初始化函数
 *         配置系统时钟为 72MHz
 * @param  无
 * @retval 无
 */
static void SystemClock_Init(void)
{
    /* 复位 RCC 并配置 HSE */
    RCC_DeInit();                      
    RCC_HSEConfig(RCC_HSE_ON);         
    while(RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET); 
    
    /* 配置 PLL: HSE 8MHz × 9 = 72MHz */
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9); 
    RCC_PLLCmd(ENABLE);                
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET); 
    
    /* 切换系统时钟源为 PLL */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK); 
    while(RCC_GetSYSCLKSource() != 0x08);      
    
    /* 配置总线分频 */
    // AHB  = 72MHz
    // APB1 = 36MHz (最大允许频率)
    // APB2 = 72MHz
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);
    
    /* 更新系统时钟变量 */
    SystemCoreClockUpdate(); 
}



/**
 * @brief  开始任务函数
 *         负责创建其他应用任务，然后删除自身
 * @param  pvParameters: 任务参数（未使用）
 * @retval 无
 */
static void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           // 进入临界区（禁止中断）
    
    BaseTaskCreate();    // 创建基础任务
    SysTaskCreate();     // 创建系统任务
   
    
    taskEXIT_CRITICAL();            // 退出临界区（恢复中断）

     /* 删除开始任务（任务已完成） */
    vTaskDelete(StartTask_Handler); 
}


/**
 * @brief  系统初始化函数
 *         完成系统时钟、中断优先级分组和各个任务模块的初始化配置
 * @param  无
 * @retval 无
 */
static void Sys_Init(void)
{
    /* 系统时钟初始化 */
    SystemClock_Init();
    /* 中断优先级分组配置 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    /* LED 端口初始化 */
    LED_Init();
    /* USART 初始化 */
    USART_Config();
    /* MAX30102 I2C 初始化 */
    MAX30102_I2C_Init();
    /* ADS1292R ECG 采集芯片初始化 — 已移至 ECG_Task 内部，调度器启动后执行 */
    // ADS1292_Init();
}



/**
 * @brief  主函数
 * @param  无
 * @retval int: 程序返回值（通常不返回）
 */
int main(void)
{
    /* 系统初始化 */
    Sys_Init();  // 调用系统初始化函数
    
    /* 创建开始任务 */
    xTaskCreate((TaskFunction_t )start_task,            
                (const char*    )"start_task",          
                (uint16_t       )START_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )START_TASK_PRIO,       
                (TaskHandle_t*  )&StartTask_Handler);   
                
    /* 启动任务调度器 */
    vTaskStartScheduler();          
    
    /* 错误处理（正常情况下不会执行到这里） */
    while(1)
    {
        // 可添加 LED 常亮等错误指示
    }
}

