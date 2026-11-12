/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : rtos_config.h
  Description : FreeRTOS 多任务集中配置文件，统一管理所有任务的优先级和堆栈大小
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

/* ================================ 头文件保护宏 ================================ */

#ifndef __TASKS_CONFIG_H
#define __TASKS_CONFIG_H


/* ================================ 依赖头文件包含区 ============================ */

#include "FreeRTOS.h"        
#include "task.h"


/* ================================ 导出宏定义区 ============================== */

/** @defgroup Task_Priority_Definitions 任务优先级定义
  * @{
  */
#define START_TASK_PRIO     1       ///< 开始任务优先级
#define LED0_TASK_PRIO      3       ///< LED0 任务优先级
#define USART_TASK_PRIO     3       ///< USART 任务优先级 
#define LED1_TASK_PRIO      3       ///< LED1 任务优先级
#define TEMP_TASK_PRIO      3       ///< TEMP 任务优先级
#define SENSOR_TASK_PRIO    2       ///< 传感器任务优先级 (略低于TEMP)
/** @} */


/** @defgroup Task_Stack_Size_Definitions 任务堆栈大小定义
  * @{
  */
#define START_STK_SIZE      128     ///< 开始任务堆栈大小
#define LED0_STK_SIZE       50      ///< LED0 任务堆栈大小
#define LED1_STK_SIZE       50      ///< LED1 任务堆栈大小
#define USART_TASK_STACK_SIZE   1024
#define SENSOR_TASK_STK_SIZE    256     ///< 传感器任务堆栈大小
/** @} */


/* ================================ 导出类型定义区 ============================ */

/* 本文件无自定义类型定义 */


/* ================================ 导出常量定义区 ============================ */

/* 本文件无自定义常量定义 */


/* ================================ 导出全局变量声明区 ======================== */

/** @defgroup Task_Handle_Declarations 任务句柄声明
  * @{
  */
extern TaskHandle_t StartTask_Handler;    ///< 开始任务句柄
extern TaskHandle_t LED0Task_Handler;     ///< LED0 任务句柄
extern TaskHandle_t LED1Task_Handler;     ///< LED1 任务句柄
extern TaskHandle_t TEMPTask_Handler;     ///< TEMP 任务句柄
extern TaskHandle_t SensorTask_Handler;   ///< 传感器任务句柄
/** @} */


/* ================================ 导出函数声明区 ============================ */
void BaseTaskCreate(void);    ///< 基础任务创建函数声明
void Sensor_Task(void *pvParameters);   ///< 传感器采集任务（V4 三帧）
void SysTaskCreate(void);     ///< 系统任务创建函数声明

#endif /* __TASKS_CONFIG_H */
