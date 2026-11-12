#ifndef _TJC_H
#define _TJC_H

#include "stm32f10x.h"
#include "usart.h"

/* TJC协议终止符：3个0xFF */
#define TJC_END_1   0xFF
#define TJC_END_2   0xFF
#define TJC_END_3   0xFF

/* 默认使用的串口 */
#define TJC_USART   USART1

/* 函数声明 */
void    TJC_SendCmd(char *cmd);
void    TJC_SetWaveform(uint8_t channel, uint8_t value);
void    TJC_SetNumber(char *component, int32_t value);
uint8_t TJC_NormalizePPG(uint16_t raw_value);

#endif
