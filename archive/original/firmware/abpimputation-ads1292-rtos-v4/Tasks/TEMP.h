#ifndef __TEMP_H
#define __TEMP_H

#include "FreeRTOS.h"
#include "task.h"
#include "MAX30205.h"

/* 温度采集任务参数 */
#define TEMP_TASK_STACK_SIZE    512         /* 任务堆栈大小（字） */
#define TEMP_TASK_PRIORITY      4           /* 任务优先级 */
#define TEMP_SAMPLE_INTERVAL    500        /* 采样间隔（毫秒） */

/* 全局温度变量（extern声明，定义在.c文件中） */
extern volatile float g_CurrentTemp;
extern volatile uint8_t g_TempErrorFlag;    /* 错误标志：0正常，1错误 */

/* 函数声明 */
void TEMP_Task(void *argument);
void TEMP_TaskCreate(void);

#endif /* __TEMP_H */
