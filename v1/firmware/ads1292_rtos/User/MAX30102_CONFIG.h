#ifndef __MAX30102_CONFIG_H
#define __MAX30102_CONFIG_H

#include "stm32f10x.h"
#include <stdint.h>

/* SCL引脚定义 PB6 */
#define MAX30102_SCL_PIN            GPIO_Pin_6
#define MAX30102_SCL_GPIO_PORT      GPIOB
#define MAX30102_SCL_GPIO_CLK       RCC_APB2Periph_GPIOB

/* SDA引脚定义 PB7 */
#define MAX30102_SDA_PIN            GPIO_Pin_7
#define MAX30102_SDA_GPIO_PORT      GPIOB
#define MAX30102_SDA_GPIO_CLK       RCC_APB2Periph_GPIOB

/* INT引脚定义 PB8 */
#define MAX30102_INT_PIN            GPIO_Pin_8
#define MAX30102_INT_GPIO_PORT      GPIOB
#define MAX30102_INT_GPIO_CLK       RCC_APB2Periph_GPIOB

/* 位带I2C位操作宏 (开漏输出模式) */
#define MAX30102_I2C_SCL_HIGH()     GPIO_SetBits(MAX30102_SCL_GPIO_PORT, MAX30102_SCL_PIN)
#define MAX30102_I2C_SCL_LOW()      GPIO_ResetBits(MAX30102_SCL_GPIO_PORT, MAX30102_SCL_PIN)
#define MAX30102_I2C_SDA_HIGH()     GPIO_SetBits(MAX30102_SDA_GPIO_PORT, MAX30102_SDA_PIN)
#define MAX30102_I2C_SDA_LOW()      GPIO_ResetBits(MAX30102_SDA_GPIO_PORT, MAX30102_SDA_PIN)
#define MAX30102_I2C_SDA_READ()     GPIO_ReadInputDataBit(MAX30102_SDA_GPIO_PORT, MAX30102_SDA_PIN)

/* INT引脚读取宏 */
#define MAX30102_INT_Read()         GPIO_ReadInputDataBit(MAX30102_INT_GPIO_PORT, MAX30102_INT_PIN)

/* MAX30102 I2C设备地址 (8位写地址) */
#define MAX30102_I2C_ADDRESS        0xAE

/* 函数声明 */
void MAX30102_I2C_Init(void);
void MAX30102_I2C_Delay(void);
void MAX30102_I2C_Start(void);
void MAX30102_I2C_Stop(void);
uint8_t MAX30102_I2C_SendByte(uint8_t byte);
uint8_t MAX30102_I2C_RecvByte(uint8_t ack);
uint8_t MAX30102_I2C_ReadByte(uint8_t device_addr, uint8_t register_addr);
uint8_t MAX30102_I2C_WriteByte(uint8_t device_addr, uint8_t register_addr, uint8_t data);
uint8_t MAX30102_I2C_ReadArray(uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t num);

void MAX30102_INT_Init(void);

#endif /* __MAX30102_CONFIG_H */
