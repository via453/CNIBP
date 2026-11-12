/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : usart.c
  Description : USART串口通信模块，支持FreeRTOS多任务环境下的串口通信
  History     :
      euin   [2026]   create
*****************************************************************/
 
/* ================================= 头文件包含区 ================================= */
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "stm32f10x.h"
#include "misc.h" 

/* ================================= 私有宏定义区 ================================= */
#define USART_RX_BUFFER_SIZE 64        /* 串口接收缓冲区大小 */

/* ================================= 私有类型定义区 ================================= */

/* ================================= 私有全局变量区 ================================= */
uint8_t usart_rx_buffer[USART_RX_BUFFER_SIZE];  /* 串口接收数据缓冲区 */
uint16_t usart_rx_index = 0;                    /* 接收缓冲区当前索引 */

/* ================================= 私有函数声明区 ================================= */

/* ================================= 私有函数实现区 ================================= */

/**
  * @brief  USART串口初始化配置
  * @param  None
  * @retval None
  * @note   初始化USART1串口，配置GPIO引脚、波特率、数据位、停止位、校验位等参数
  *         并配置中断优先级，使能串口收发功能
  */
void USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* 1. 使能USART和GPIO时钟 */
    RCC_APB2PeriphClockCmd(USARTx_CLK | USARTx_GPIO_CLK, ENABLE);

    /* 2. 配置TX引脚（PA9）：复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = USARTx_TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  /* 复用推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(USARTx_GPIO_PORT, &GPIO_InitStructure);

    /* 3. 配置RX引脚（PA10）：浮空输入 */
    GPIO_InitStructure.GPIO_Pin = USARTx_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  /* 浮空输入 */
    GPIO_Init(USARTx_GPIO_PORT, &GPIO_InitStructure);

    /* 4. 配置USART通信参数 */
    USART_InitStructure.USART_BaudRate = USARTx_BAUDRATE;                    /* 波特率 */
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;              /* 8位数据位 */
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                   /* 1位停止位 */
    USART_InitStructure.USART_Parity = USART_Parity_No;                      /* 无校验位 */
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; /* 无硬件流控 */
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;          /* 收发模式 */
    USART_Init(USARTx, &USART_InitStructure);

    /* 5. 配置USART接收中断 */
    USART_ITConfig(USARTx, USART_IT_RXNE, ENABLE);  /* 使能接收非空中断 */

    /* 6. 配置NVIC中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = USARTx_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;  /* 抢占优先级5 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         /* 子优先级0 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            /* 使能中断通道 */
    NVIC_Init(&NVIC_InitStructure);

    /* 7. 使能USART */
    USART_Cmd(USARTx, ENABLE);
}


/**
  * @brief  USART1中断服务函数
  * @param  None
  * @retval None
  * @note   处理USART1接收中断，实现数据接收和缓冲区管理
  *         支持换行符检测和缓冲区溢出保护
  */
void USART1_IRQHandler(void)
{
    /* 检查接收数据寄存器非空中断标志 */
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(USART1);
        
        /* 检查缓冲区是否溢出 */
        if (usart_rx_index < USART_RX_BUFFER_SIZE - 1)
        {
            /* 将接收到的字符存入缓冲区 */
            usart_rx_buffer[usart_rx_index++] = ch;
            
            /* 处理换行符，表示一条完整消息接收完成 */
            if (ch == '\n' || ch == '\r')
            {
                usart_rx_buffer[usart_rx_index] = '\0';  /* 添加字符串结束符 */
                /* 可以在这里添加消息处理逻辑 */
                usart_rx_index = 0;  /* 重置缓冲区索引 */
            }
        }
        else
        {
            /* 缓冲区溢出处理：重置缓冲区索引 */
            usart_rx_index = 0;
        }
        
        /* 清除中断标志 */
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}


/* ================================= 导出函数实现区 ================================= */

#pragma import(__use_no_semihosting)             

struct __FILE 
{ 
    int handle; 
}; 

FILE __stdout;       

/**
  * @brief  系统退出函数（重定向）
  * @param  x: 退出码
  * @retval None
  */
void _sys_exit(int x) 
{ 
    x = x; 
} 

/**
  * @brief  重定向C库函数printf到USART1
  * @param  ch: 要发送的字符
  * @param  f: 文件指针
  * @retval int: 发送的字符
  * @note   实现printf函数的重定向，将标准输出重定向到USART1串口
  */
int fputc(int ch, FILE *f) 
{
    /* 发送一个字节数据到USART1 */
    USART_SendData(USART1, (uint8_t) ch);

    /* 等待发送寄存器空标志，确保数据发送完成 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    return (ch);
}

/**
  * @brief  USART测试任务函数
  * @param  pvParameters: 任务参数指针
  * @retval None
  * @note   FreeRTOS任务函数，用于测试USART串口通信功能
  *         周期性发送测试数据，验证串口工作状态
  */
void usart_task(void *pvParameters)
{
    uint32_t test_count = 0;  /* 测试计数器 */
  
    /* 任务主循环 */
    for (;;)
    {
        /* 打印测试信息 */
        printf("FreeRTOS USART Test Task: Count = %d\r\n", test_count++);
        
        /* 使用FreeRTOS任务延时，让出CPU使用权 */
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        /* 每5次循环打印一次特殊测试信息 */
        if (test_count % 5 == 0)
        {
            printf("--- Test Float: %.2f, Hex: 0x%04X ---\r\n", 3.1415f, 0x1234);
        }
    }
}

/**
  * @brief  批量发送数据到USART1（用于协议帧发送）
  * @param  buf: 数据缓冲区指针
  * @param  len: 数据长度
  * @retval None
  */
void USART_WriteBuf(uint8_t *buf, uint16_t len)
{
    for(uint16_t i = 0; i < len; i++)
    {
        USART_SendData(USART1, buf[i]);
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    }
}
