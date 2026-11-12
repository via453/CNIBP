#include "TEMP.h"
#include <stdio.h>
#include "usart.h"

/* 无 DEBUG_OUTPUT 时静默 MAX30205 调试输出 */
#ifndef DEBUG_OUTPUT
#undef MAX30205_DEBUG_ENABLE
#define MAX30205_DEBUG_ENABLE 0
#endif
/* 全局变量定义 */
volatile float g_CurrentTemp = 0.0f;        /* 当前温度值 */
volatile uint8_t g_TempErrorFlag = 0;      /* 温度读取错误标志 */

/* 任务句柄（可选，用于外部控制） */
static TaskHandle_t xTempTaskHandle = NULL;

/**
  * @brief  温度采集 FreeRTOS 任务
  * @param  argument: 任务参数（未使用）
  * @retval 无
  * @note   任务流程：
  *         1. 初始化 I2C 硬件
  *         2. 初始化 MAX30205 传感器
  *         3. 进入循环，每秒读取一次温度
  *         4. 更新全局变量 g_CurrentTemp
  *         5. 延时 1000ms
  */
void TEMP_Task(void *argument)
{
    uint8_t initStatus;
    float tempValue;
    
    (void)argument; /* 避免未使用参数警告 */

    /* 步骤 1：初始化 I2C 硬件接口 */
    MX_TEMP_I2C_Init();
    MAX30205_DEBUG_PRINT("[TEMP] I2C hardware initialization complete\r\n");
    
    /* 步骤 2：初始化 MAX30205 传感器 */
    /* 增加重试机制，确保传感器正确初始化 */
    MAX30205_DEBUG_PRINT("[TEMP] Starting MAX30205 sensor initialization...\r\n");
    initStatus = 1;
    for(uint8_t retry = 0; retry < 3; retry++)
    {
        MAX30205_DEBUG_PRINT("[TEMP] Initialization attempt %d/3\r\n", retry + 1);
        initStatus = MAX30205_Init();
        if(initStatus == 0)
        {
            MAX30205_DEBUG_PRINT("[TEMP] MAX30205 initialized successfully on attempt %d\r\n", retry + 1);
            break;  /* 初始化成功，跳出重试循环 */
        }
        MAX30205_DEBUG_PRINT("[TEMP] MAX30205 initialization failed on attempt %d, retrying in 100ms...\r\n", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(100));   /* 延时 100ms 后重试 */
    }
    
    if(initStatus != 0)
    {
        MAX30205_DEBUG_PRINT("[TEMP] ERROR: Failed to initialize MAX30205 after 3 attempts!\r\n");
    }
    
    /* 步骤 3：正常采集循环 */
    while(1)
    {
        /* 读取温度值 */
        tempValue = MAX30205_ReadTemp();
        /* 检查读取是否成功（返回值不是错误码 -999.0f） */
        if(tempValue > -900.0f)   /* 正常温度范围应该在 -40°C 到 +85°C 之间 */
        {
            g_CurrentTemp = tempValue;
            g_TempErrorFlag = 0;
            
            /* 此处可添加：通过队列发送温度数据给其他任务 */
            /* xQueueSend(xTempQueue, &tempValue, portMAX_DELAY); */
#ifdef OUTPUT_TJC
            /* TJC 模式: 发送温度值×10 到数值控件 n0（如 36.5→365） */
            TJC_SendValue("n0", (int32_t)(tempValue * 10));
#else
            printf("Temp: %.2f C\r\n", tempValue);
#endif
            
        }
        else
        {
            /* 读取失败 */
            g_TempErrorFlag = 1;
            MAX30205_DEBUG_PRINT("[TEMP] ERROR: Temperature read failed! Error code: %.2f\r\n", tempValue);
            MAX30205_DEBUG_PRINT("[TEMP] This indicates a communication error or invalid sensor configuration\r\n");
            
            // 在错误发生时也尝试恢复总线
            I2C_CheckAndRecoverBus();
        }
        
        /* 每秒读取一次 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
  * @brief  创建温度采集任务
  * @param  无
  * @retval 无
  */
void TEMP_TaskCreate(void)
{
#ifdef DEBUG_OUTPUT
    printf("[TEMP] Creating Temperature Task (Priority: %d, Stack: %d)\r\n", 
           TEMP_TASK_PRIORITY, TEMP_TASK_STACK_SIZE);
#endif
           
    xTaskCreate(
        TEMP_Task,                  /* 任务函数 */
        "TempTask",                 /* 任务名称 */
        TEMP_TASK_STACK_SIZE,       /* 堆栈深度 */
        NULL,                       /* 任务参数 */
        TEMP_TASK_PRIORITY,         /* 优先级 */
        &xTempTaskHandle            /* 任务句柄 */
    );
    
#ifdef DEBUG_OUTPUT
    if(xTempTaskHandle != NULL)
    {
        printf("[TEMP] Temperature Task created successfully\r\n");
    }
    else
    {
        printf("[TEMP] ERROR: Failed to create Temperature Task!\r\n");
    }
#endif
}