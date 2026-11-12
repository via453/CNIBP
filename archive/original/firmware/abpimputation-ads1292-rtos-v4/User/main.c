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
/* ================================ 全局变量定义区 ============================== */


/* ================================ V4 协议帧发送函数 ============================ */

/* 外部传感器读取函数（由用户实现） */
extern int32_t ADS1292_ReadECG(void);           // 返回 uV
extern void    MAX30102_Read(uint32_t *ir, uint32_t *red);  // 返回 IR, RED

/**
  * @brief  发送 ECG 数据帧 (0xAA, 16字节)
  * @param  uV: ECG 值 (uV)
  * @param  hr: 心率
  */
static void SendECGPacket(int32_t uV, uint8_t hr)
{
    uint8_t pkt[16];
    uint32_t ts = xTaskGetTickCount();  // 毫秒时间戳
    pkt[0] = 0xAA; pkt[1] = 0xAA; pkt[2] = 0xF3; pkt[3] = 0x07;
    pkt[4] = (uV >> 24) & 0xFF;
    pkt[5] = (uV >> 16) & 0xFF;
    pkt[6] = (uV >> 8) & 0xFF;
    pkt[7] = uV & 0xFF;
    pkt[8] = hr;
    pkt[9] = 0;    /* status */
    pkt[10] = 0;   /* cmrr */
    pkt[11] = (ts >> 24) & 0xFF;
    pkt[12] = (ts >> 16) & 0xFF;
    pkt[13] = (ts >> 8) & 0xFF;
    pkt[14] = ts & 0xFF;
    pkt[15] = 0;
    for(int i = 0; i < 15; i++) pkt[15] ^= pkt[i];  /* XOR CRC */
    USART_WriteBuf(pkt, 16);
}

/**
  * @brief  发送 PPG 数据帧 (0xBB, 17字节)
  * @param  ir: 红外值
  * @param  red: 红光值
  */
static void SendPPGPacket(uint32_t ir, uint32_t red)
{
    uint8_t pkt[17];
    uint32_t ts = xTaskGetTickCount();
    pkt[0] = 0xBB; pkt[1] = 0xBB; pkt[2] = 0xF2; pkt[3] = 0x0C;
    pkt[4] = (ir >> 24) & 0xFF;
    pkt[5] = (ir >> 16) & 0xFF;
    pkt[6] = (ir >> 8) & 0xFF;
    pkt[7] = ir & 0xFF;
    pkt[8] = (red >> 24) & 0xFF;
    pkt[9] = (red >> 16) & 0xFF;
    pkt[10] = (red >> 8) & 0xFF;
    pkt[11] = red & 0xFF;
    pkt[12] = (ts >> 24) & 0xFF;
    pkt[13] = (ts >> 16) & 0xFF;
    pkt[14] = (ts >> 8) & 0xFF;
    pkt[15] = ts & 0xFF;
    pkt[16] = 0;
    for(int i = 0; i < 16; i++) pkt[16] ^= pkt[i];
    USART_WriteBuf(pkt, 17);
}

/**
  * @brief  发送 TEMP 数据帧 (0xCC, 5字节)
  * @param  temp_c: 温度值 (℃)
  */
static void SendTEMPPacket(float temp_c)
{
    int16_t raw = (int16_t)(temp_c * 100.0f + 0.5f);
    uint8_t pkt[5];
    pkt[0] = 0xCC; pkt[1] = 0xCC;
    pkt[2] = (raw >> 8) & 0xFF;
    pkt[3] = raw & 0xFF;
    pkt[4] = pkt[0] ^ pkt[1] ^ pkt[2] ^ pkt[3];
    USART_WriteBuf(pkt, 5);
}


/* ================================ 函数声明区 ================================ */

static void SystemClock_Init(void);         ///< 系统时钟初始化
static void start_task(void *pvParameters); ///< 开始任务函数
static void Sys_Init(void);                 ///< 系统初始化函数
static void Sensor_Task(void *pvParameters); ///< V4 传感器任务

/* ================================ 函数实现区 ================================ */

/**
  * @brief  V4 传感器任务 (250Hz)
  *         统一读取 ECG、PPG、TEMP 并按要求格式发送。
  * @param  pvParameters: 任务参数（未使用）
  */
static void Sensor_Task(void *pvParameters)
{
    uint32_t ecg_val, ir_val, red_val;
    float temp_c;
    uint32_t last_temp_tick = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    (void)pvParameters;

    for (;;)
    {
        uint32_t now = xTaskGetTickCount();  /* 统一时间戳 */

        /* 1. 读取传感器 */
        ecg_val = ADS1292_ReadECG();          /* 用户实现: 读ADS1292 */
        MAX30102_Read(&ir_val, &red_val);     /* 用户实现: 读MAX30102 */
        temp_c = g_CurrentTemp;               /* 温度任务已经更新 */

        /* 2. 发送协议帧 */
        SendECGPacket((int32_t)ecg_val, 0);   /* ECG @250Hz */
        SendPPGPacket(ir_val, red_val);       /* PPG @250Hz (由Python侧降采样到100Hz) */

        /* 3. 体温 @1Hz */
        if (now - last_temp_tick >= 1000)
        {
            SendTEMPPacket(temp_c);
            last_temp_tick = now;
        }

        /* 4. 等待下一周期 (250Hz = 4ms) */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(4));
    }
}

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

