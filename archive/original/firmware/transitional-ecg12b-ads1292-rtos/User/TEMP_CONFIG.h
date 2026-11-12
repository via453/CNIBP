#ifndef __TEMP_CONFIG_H
#define __TEMP_CONFIG_H

#include "stm32f10x.h"

/* SCL引脚定义 PC5 */
#define TEMP_SCL_PIN            GPIO_Pin_5
#define TEMP_SCL_GPIO_PORT      GPIOC

/* SDA引脚定义 PC4 */
#define TEMP_SDA_PIN            GPIO_Pin_4
#define TEMP_SDA_GPIO_PORT      GPIOC

/* 位带I2C位操作宏 (开漏输出模式) */
#define I2C_BB_SCL_HIGH()       GPIO_SetBits(TEMP_SCL_GPIO_PORT, TEMP_SCL_PIN)
#define I2C_BB_SCL_LOW()        GPIO_ResetBits(TEMP_SCL_GPIO_PORT, TEMP_SCL_PIN)
#define I2C_BB_SDA_HIGH()       GPIO_SetBits(TEMP_SDA_GPIO_PORT, TEMP_SDA_PIN)
#define I2C_BB_SDA_LOW()        GPIO_ResetBits(TEMP_SDA_GPIO_PORT, TEMP_SDA_PIN)
#define I2C_BB_SDA_READ()       GPIO_ReadInputDataBit(TEMP_SDA_GPIO_PORT, TEMP_SDA_PIN)

/* MAX30205 I2C地址 */
#define MAX30205_I2C_ADDRESS    0x48

/* 函数声明 */
void MX_TEMP_I2C_Init(void);
void I2C_BB_Delay(void);
void I2C_BB_Start(void);
void I2C_BB_Stop(void);
uint8_t I2C_BB_SendByte(uint8_t byte);
uint8_t I2C_BB_RecvByte(uint8_t ack);
uint8_t I2C_CheckAndRecoverBus(void);

#endif /* __TEMP_CONFIG_H */
