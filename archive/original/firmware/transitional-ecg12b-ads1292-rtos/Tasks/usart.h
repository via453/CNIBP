/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : usart.h
  Description : USART串口通信模块头文件，定义串口配置参数和函数接口
  History     :
      euin   [2026]   create
*****************************************************************/

/* ================================= 头文件保护宏 ================================= */
#ifndef __USART_H
#define __USART_H

/* ================================= 依赖头文件包含区 ================================= */
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ================================= 导出宏定义区 ================================= */
#define USARTx                   USART1        /* 选择USART1 */
#define USARTx_CLK               RCC_APB2Periph_USART1
#define USARTx_GPIO_CLK          RCC_APB2Periph_GPIOA
#define USARTx_TX_PIN            GPIO_Pin_9
#define USARTx_RX_PIN            GPIO_Pin_10
#define USARTx_GPIO_PORT         GPIOA
#define USARTx_BAUDRATE          115200        /* 波特率 */
#define USARTx_IRQn              USART1_IRQn

/* 重定向printf需要的宏（兼容MDK的MicroLIB和GCC编译器） */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

//----------------------------- 串口输出模式选择 -----------------------------//

/* TJC 串口屏输出模式（取消注释以启用 TJC 协议输出） */
#define OUTPUT_TJC

/* CSV 原始数据输出模式（取消注释以输出原始浮点数值，与 OUTPUT_TJC 二选一） */
// #define CSV_OUTPUT


//----------------------------- 数据处理选项 -----------------------------//

// /* demo数据模式：使用kai采集数据(取消注释) VS EUIN演示数据（适用于所有输出模式） */
#define REALTIME_DATA

/* 真实数据幅度缩放倍数（仅在 REALTIME_DATA 模式下生效） */
#define REALTIME_SCALE  1000


//----------------------------- 数据输出样式 -----------------------------//

/* 演示数据幅度缩放倍数 */
#define DEMO_SCALE  0.5f

/* 演示数据滚动速度（Hz），TJC/二进制模式建议50Hz，CSV模式建议250Hz */
#define DEMO_HZ  120

/* 演示数据平滑滤波（取消注释启用滑动平均滤波） */
#define ENABLE_SMOOTH_FILTER

/* 平滑滤波窗口大小（点数），值越大越平滑 */
#define SMOOTH_WINDOW  50

/* 原始未滤波数据模式（取消注释以使用未滤波原始ECG数据，与REALTIME_DATA二选一） */
// #define RAW_DATA
/* 原始数据幅度缩放倍数 */
#define RAW_SCALE  1000




/* 心率检测（取消注释启用R峰检测与心率计算） */
#define ENABLE_HR_DETECTION
/* R峰检测阈值（归一化值 0~255） */
#define R_PEAK_THRESHOLD  180
/* R峰结束阈值（低于此值确认R峰结束） */
#define R_PEAK_END_THRESHOLD  100
/* RR间隔最小值（采样点数，防T波误检） */
#define MIN_RR_SAMPLES  15
/* RR间隔最大值（采样点数） */
#define MAX_RR_SAMPLES  300
/* HR滑动平均窗口（RR间隔数） */
#define HR_AVG_WINDOW  5

/* 调试输出模式（取消注释以启用调试 printf，TJC 模式下建议关闭） */
// #define DEBUG_OUTPUT

/* ================================= 导出类型定义区 ================================= */

/* ================================= 导出常量定义区 ================================= */

/* ================================= 导出全局变量声明区 ================================= */
extern SemaphoreHandle_t xUSART1_Semaphore;

/* ================================= 导出函数声明区 ================================= */
void USART_Config(void);
void usart_task(void *pvParameters);

/* DMA相关函数 */
void DMA_Config(DMA_Channel_TypeDef *DMA_CHx, uint32_t cpar, uint32_t cmar);
void DMA_Enable(DMA_Channel_TypeDef *DMA_CHx, uint16_t len);

/* 串口发送函数 */
void USARTx_Send(USART_TypeDef *USART_Port, uint8_t *Data, uint8_t Len);
void UART_SendECGPacketLMS(int32_t ecg_uV, uint8_t hr, uint8_t status, uint8_t cmrr);

/* TJC 串口屏辅助函数 */
#ifdef OUTPUT_TJC
void TJC_SendWaveform(uint8_t id, uint8_t value);
void TJC_SendValue(const char *component, int32_t value);
#endif

#endif /* __USART_H */