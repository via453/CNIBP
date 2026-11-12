#ifndef __SPO2_H
#define __SPO2_H

#include "FreeRTOS.h"
#include "task.h"
#include "MAX30102.h"
#include "blood.h"

/* SPO2 任务参数 */
#define SPO2_TASK_STACK_SIZE    1024        /* 任务堆栈大小（字），FFT+printf需要较大栈 */
#define SPO2_TASK_PRIORITY      3           /* 任务优先级 */

/* 函数声明 */
void SPO2_Task(void *argument);
void SPO2_TaskCreate(void);

#endif /* __SPO2_H */
