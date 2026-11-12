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

/* ================================= 导出类型定义区 ================================= */

/* ================================= 导出常量定义区 ================================= */

/* ================================= 导出全局变量声明区 ================================= */

/* ================================= 导出函数声明区 ================================= */
void USART_Config(void);
void USART_WriteBuf(uint8_t *buf, uint16_t len);
void usart_task(void *pvParameters);

#endif /* __USART_H */