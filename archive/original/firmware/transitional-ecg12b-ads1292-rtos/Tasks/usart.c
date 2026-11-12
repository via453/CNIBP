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
SemaphoreHandle_t xUSART1_Semaphore = NULL;     /* USART1 发送二值信号量 */

/* ================================= 私有函数声明区 ================================= */

/* ================================= 私有函数实现区 ================================= */

/**
  * @brief  DMA通道配置函数
  * @param  DMA_CHx: DMA通道，如DMA1_Channel4
  * @param  cpar: 外设寄存器地址
  * @param  cmar: 存储器地址
  * @retval None
  * @note   配置DMA通道：存储器到外设模式/8位数据宽度/存储器地址递增
  */
void DMA_Config(DMA_Channel_TypeDef *DMA_CHx, u32 cpar, u32 cmar)
{
    RCC->AHBENR |= 1 << 0;           /* 使能DMA1时钟 */

    DMA_CHx->CPAR = (u32)cpar;       /* 设定外设寄存器地址（发送地址） */
    DMA_CHx->CMAR = (u32)cmar;       /* 设定数据存储器地址（接收地址） */

    DMA_CHx->CCR = 0;                /* 复位控制寄存器 */

    /* 禁用所有中断 */
    DMA_CHx->CCR |= 0 << 1;          /* 传输完成中断：禁止 */
    DMA_CHx->CCR |= 0 << 2;          /* 半传输中断：禁止 */
    DMA_CHx->CCR |= 0 << 3;          /* 传输错误中断：禁止 */

    /* 设定数据传输方向：从存储器到外设 */
    DMA_CHx->CCR |= 1 << 4;          /* 1: 从存储器到外设 */
    DMA_CHx->CCR |= 0 << 5;          /* 0: 非循环模式 */

    /* 设定外设/存储器地址递增 */
    DMA_CHx->CCR |= 0 << 6;          /* 0: 外设地址不递增 */
    DMA_CHx->CCR |= 1 << 7;          /* 1: 存储器地址递增 */

    /* 设定外设数据宽度：8位 */
    DMA_CHx->CCR |= 0 << 8;
    DMA_CHx->CCR |= 0 << 9;          /* 00: 8位 */

    /* 设定存储器数据宽度：8位 */
    DMA_CHx->CCR |= 0 << 10;
    DMA_CHx->CCR |= 0 << 11;         /* 00: 8位 */

    /* 设定通道优先级：中等 */
    DMA_CHx->CCR |= 0 << 12;
    DMA_CHx->CCR |= 0 << 13;         /* 00: 中等优先级 */

    /* 存储器到存储器模式：禁用 */
    DMA_CHx->CCR |= 0 << 14;
}

/**
  * @brief  启动一次DMA传输
  * @param  DMA_CHx: DMA通道，如DMA1_Channel4
  * @param  len: 本次传输的数据量（字节数）
  * @retval None
  * @note   关闭当前DMA传输，设置传输数量后重新启动
  */
void DMA_Enable(DMA_Channel_TypeDef *DMA_CHx, u16 len)
{
    DMA_CHx->CCR &= ~(1 << 0);       /* 关闭当前DMA传输 */
    while (DMA_CHx->CCR & (1 << 0)); /* 等待EN位清除 */
    DMA_CHx->CNDTR = len;            /* 设置数据传输数量 */
    DMA_CHx->CCR |= 1 << 0;          /* 启动DMA传输 */
}

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

    /* 8. 创建USART1发送二值信号量（初始状态=可用） */
    xUSART1_Semaphore = xSemaphoreCreateBinary();
    if (xUSART1_Semaphore != NULL)
    {
        xSemaphoreGive(xUSART1_Semaphore);
    }
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
    /* 获取信号量保护 */
    if (xUSART1_Semaphore != NULL)
    {
        xSemaphoreTake(xUSART1_Semaphore, portMAX_DELAY);
    }

    /* 发送一个字节数据到USART1 */
    USART_SendData(USART1, (uint8_t) ch);

    /* 等待发送寄存器空标志，确保数据发送完成 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* 释放信号量 */
    if (xUSART1_Semaphore != NULL)
    {
        xSemaphoreGive(xUSART1_Semaphore);
    }

    return (ch);
}

/**
  * @brief  串口发送字节串函数（轮询方式）
  * @param  USARTx: 串口号，如USART1
  * @param  Data: 待发送数据缓冲区指针
  * @param  Len: 待发送数据长度（字节数）
  * @retval None
  * @note   逐字节发送，等待发送完成标志后发送下一字节
  */
void USARTx_Send(USART_TypeDef *USART_Port, uint8_t *Data, uint8_t Len)
{
    uint8_t i = 0;

    /* 获取信号量（保护整个发送过程为原子操作） */
    if (xUSART1_Semaphore != NULL)
    {
        xSemaphoreTake(xUSART1_Semaphore, portMAX_DELAY);
    }

    for (i = 0; i < Len; i++)
    {
        while (USART_GetFlagStatus(USART_Port, USART_FLAG_TC) != SET);
        USART_SendData(USART_Port, Data[i]);
    }

    /* 释放信号量 */
    if (xUSART1_Semaphore != NULL)
    {
        xSemaphoreGive(xUSART1_Semaphore);
    }
}

/**
  * @brief  发送LMS滤波后的ECG数据包（12字节）
  * @param  ecg_uV: ECG微伏值（32位有符号整数）
  * @param  hr: 心率值（8位无符号）
  * @param  status: 状态字节（8位无符号）
  * @param  cmrr: 共模抑制比字节（8位无符号）
  * @retval None
  * @note   数据包格式：0xAA 0xAA 0xF3 0x07 + ECG(4B) + HR + status + CMRR + checksum
  */
void UART_SendECGPacketLMS(int32_t ecg_uV, uint8_t hr, uint8_t status, uint8_t cmrr)
{
    uint8_t packet[12];
    uint8_t i, sum = 0;

    packet[0]  = 0xAA;
    packet[1]  = 0xAA;
    packet[2]  = 0xF3;
    packet[3]  = 0x07;
    packet[4]  = (ecg_uV >> 24) & 0xFF;
    packet[5]  = (ecg_uV >> 16) & 0xFF;
    packet[6]  = (ecg_uV >> 8) & 0xFF;
    packet[7]  = ecg_uV & 0xFF;
    packet[8]  = hr;
    packet[9]  = status;
    packet[10] = cmrr;
    for (i = 0; i < 11; i++)
    {
        sum += packet[i];
    }
    packet[11] = sum;
    USARTx_Send(USART1, packet, 12);
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

#ifdef OUTPUT_TJC
/**
  * @brief  向 TJC 串口屏发送波形数据点
  * @param  id: 波形控件 ID（0=s0, 1=s1）
  * @param  value: 数据值（0-255）
  * @retval 无
  * @note   格式: "add s{id}.id,0,{value}" + 0xFF 0xFF 0xFF
  */
void TJC_SendWaveform(uint8_t id, uint8_t value)
{
    char buf[32];
    uint8_t len;
    
    len = snprintf(buf, sizeof(buf), "add s%u.id,0,%u", id, value);
    USARTx_Send(USART1, (uint8_t *)buf, len);
    
    uint8_t tail[3] = {0xFF, 0xFF, 0xFF};
    USARTx_Send(USART1, tail, 3);
}

/**
  * @brief  向 TJC 串口屏发送数值控件值
  * @param  component: 控件名称（如 "n0", "n1"）
  * @param  value: 要显示的数值
  * @retval 无
  * @note   格式: "{component}.val={value}" + 0xFF 0xFF 0xFF
  */
void TJC_SendValue(const char *component, int32_t value)
{
    char buf[32];
    uint8_t len;
    
    len = snprintf(buf, sizeof(buf), "%s.val=%ld", component, (long)value);
    USARTx_Send(USART1, (uint8_t *)buf, len);
    
    uint8_t tail[3] = {0xFF, 0xFF, 0xFF};
    USARTx_Send(USART1, tail, 3);
}
#endif /* OUTPUT_TJC */
