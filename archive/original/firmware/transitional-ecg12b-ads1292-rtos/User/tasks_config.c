/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : tasks_config.c
  Description : FreeRTOS 任务配置源文件，提供任务句柄定义和任务创建功能
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

/* ================================ 头文件包含区 ================================ */

#include "tasks_config.h"                   // 任务配置头文件
#include "LED.h"                            // LED 任务函数声明
#include "usart.h"
#include "TEMP.h"                            // TEMP 任务函数声明
#include "SPO2.h"                            // SPO2 任务函数声明
#include "ECG.h"                             // ECG 任务函数声明

/* ================================ 全局变量定义区 ============================== */

/** @brief 任务句柄定义 */
TaskHandle_t StartTask_Handler = NULL;    ///< 开始任务句柄
TaskHandle_t LED0Task_Handler  = NULL;    ///< LED0 任务句柄
TaskHandle_t LED1Task_Handler  = NULL;    ///< LED1 任务句柄
TaskHandle_t TEMPTask_Handler  = NULL;    ///< TEMP 任务句柄
TaskHandle_t SPO2Task_Handler  = NULL;    ///< SPO2 任务句柄
TaskHandle_t ECGTask_Handler   = NULL;    ///< ECG 任务句柄


/* ================================ 函数实现区 ================================== */

/**
 * @brief  基础任务创建
 * @param  无
 * @retval 无
 */
void BaseTaskCreate(void)
{
    // /* 创建 LED0 闪烁任务 */
    // xTaskCreate((TaskFunction_t )led0_task,     
    //             (const char*    )"led0_task",   
    //             (uint16_t       )LED0_STK_SIZE, 
    //             (void*          )NULL,				
    //             (UBaseType_t    )LED0_TASK_PRIO,	
    //             (TaskHandle_t*  )&LED0Task_Handler);   
    
    // /* 创建 LED1 闪烁任务 */
    // xTaskCreate((TaskFunction_t )led1_task,     
    //             (const char*    )"led1_task",   
    //             (uint16_t       )LED1_STK_SIZE, 
    //             (void*          )NULL,
    //             (UBaseType_t    )LED1_TASK_PRIO,
    //             (TaskHandle_t*  )&LED1Task_Handler);      

    // xTaskCreate((TaskFunction_t )usart_task,           // 任务函数
    //             (const char*    )"USART_Test_Task",    // 任务名称（仅调试用）
    //             (uint16_t       )USART_TASK_STACK_SIZE,// 栈大小
    //             (void*          )NULL,                 // 任务参数（这里不需要）
    //             (UBaseType_t    )USART_TASK_PRIO,      // 优先级
    //             (TaskHandle_t*  )NULL);                // 任务句柄（可选）

    // xTaskCreate((TaskFunction_t )TEMP_Task,            // 任务函数
    //             (const char*    )"TEMP_Test_Task",     // 任务名称（仅调试用）
    //             (uint16_t       )TEMP_TASK_STACK_SIZE, // 栈大小
    //             (void*          )NULL,                 // 任务参数（这里不需要）
    //             (UBaseType_t    )TEMP_TASK_PRIO,       // 优先级
    //             (TaskHandle_t*  )&TEMPTask_Handler);   // 任务句柄（可选）

    xTaskCreate((TaskFunction_t )SPO2_Task,            // 任务函数
                (const char*    )"SPO2_Task",           // 任务名称（仅调试用）
                (uint16_t       )SPO2_STK_SIZE,        // 栈大小
                (void*          )NULL,                 // 任务参数（这里不需要）
                (UBaseType_t    )SPO2_TASK_PRIO,       // 优先级
                (TaskHandle_t*  )&SPO2Task_Handler);   // 任务句柄（可选）

    // xTaskCreate((TaskFunction_t )ECG_Task,             // 任务函数
    //             (const char*    )"ECG_Task",            // 任务名称（仅调试用）
    //             (uint16_t       )ECG_STK_SIZE,         // 栈大小
    //             (void*          )NULL,                 // 任务参数（这里不需要）
    //             (UBaseType_t    )ECG_TASK_PRIO,        // 优先级
    //             (TaskHandle_t*  )&ECGTask_Handler);    // 任务句柄（可选）
}

/**
 * @brief  系统任务创建
 *         创建系统级任务
 * @param  无
 * @retval 无
 */
void SysTaskCreate(void)
{
    
}
