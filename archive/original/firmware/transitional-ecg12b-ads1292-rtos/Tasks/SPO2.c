#include "SPO2.h"
#include <stdio.h>

/* 任务句柄 */
static TaskHandle_t xSPO2TaskHandle = NULL;

/**
  * @brief  血氧采集 FreeRTOS 任务
  * @param  argument: 任务参数（未使用）
  * @retval 无
  * @note   任务流程：
  *         1. 初始化 MAX30102 GPIO
  *         2. 检测 MAX30102 传感器
  *         3. 复位 MAX30102
  *         4. 配置 MAX30102
  *         5. 进入循环，持续调用 blood_Loop() 采集血氧数据
  */
void SPO2_Task(void *argument)
{
    uint8_t initStatus;

    (void)argument; /* 避免未使用参数警告 */

    /* 步骤 1：初始化 MAX30102 GPIO */
    printf("[SPO2] Initializing MAX30102 GPIO...\r\n");
    MAX30102_GPIO();

    /* 步骤 2-4：初始化 MAX30102 传感器（带重试机制） */
    printf("[SPO2] Starting MAX30102 sensor initialization...\r\n");
    initStatus = 1;
    for (uint8_t retry = 0; retry < 3; retry++)
    {
        printf("[SPO2] Initialization attempt %d/3\r\n", retry + 1);

        /* 检测传感器 */
        initStatus = MAX30102_Check();
        if (initStatus != 0)
        {
            printf("[SPO2] MAX30102 not detected on attempt %d\r\n", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 复位传感器 */
        initStatus = Max30102_reset();
        if (initStatus == 0)
        {
            printf("[SPO2] MAX30102 reset failed on attempt %d\r\n", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 配置传感器 */
        MAX30102_Config();
        printf("[SPO2] MAX30102 initialized successfully on attempt %d\r\n", retry + 1);
        initStatus = 0;
        break;
    }

    if (initStatus != 0)
    {
        printf("[SPO2] ERROR: Failed to initialize MAX30102 after 3 attempts!\r\n");
    }

    /* 步骤 5：正常采集循环 */
    while (1)
    {
        blood_Loop();
    }
}

/**
  * @brief  创建血氧采集任务
  * @param  无
  * @retval 无
  */
void SPO2_TaskCreate(void)
{
    printf("[SPO2] Creating SPO2 Task (Priority: %d, Stack: %d)\r\n",
           SPO2_TASK_PRIORITY, SPO2_TASK_STACK_SIZE);

    xTaskCreate(
        SPO2_Task,                  /* 任务函数 */
        "SPO2Task",                 /* 任务名称 */
        SPO2_TASK_STACK_SIZE,       /* 堆栈深度 */
        NULL,                       /* 任务参数 */
        SPO2_TASK_PRIORITY,         /* 优先级 */
        &xSPO2TaskHandle            /* 任务句柄 */
    );

    if (xSPO2TaskHandle != NULL)
    {
        printf("[SPO2] SPO2 Task created successfully\r\n");
    }
    else
    {
        printf("[SPO2] ERROR: Failed to create SPO2 Task!\r\n");
    }
}
