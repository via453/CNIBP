#include "TEMP_CONFIG.h"
#include "MAX30205.h"

void MX_TEMP_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = TEMP_SCL_PIN | TEMP_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TEMP_SCL_GPIO_PORT, &GPIO_InitStructure);

    I2C_BB_SCL_HIGH();
    I2C_BB_SDA_HIGH();
}

void I2C_BB_Delay(void)
{
    for(volatile uint32_t i = 0; i < 120; i++);
}

void I2C_BB_Start(void)
{
    I2C_BB_SDA_HIGH();
    I2C_BB_SCL_HIGH();
    I2C_BB_Delay();
    I2C_BB_SDA_LOW();
    I2C_BB_Delay();
    I2C_BB_SCL_LOW();
}

void I2C_BB_Stop(void)
{
    I2C_BB_SDA_LOW();
    I2C_BB_SCL_HIGH();
    I2C_BB_Delay();
    I2C_BB_SDA_HIGH();
    I2C_BB_Delay();
}

uint8_t I2C_BB_SendByte(uint8_t byte)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        if(byte & 0x80)
            I2C_BB_SDA_HIGH();
        else
            I2C_BB_SDA_LOW();
        I2C_BB_Delay();
        I2C_BB_SCL_HIGH();
        I2C_BB_Delay();
        I2C_BB_SCL_LOW();
        byte <<= 1;
    }

    I2C_BB_SDA_HIGH();
    I2C_BB_Delay();
    I2C_BB_SCL_HIGH();
    I2C_BB_Delay();
    uint8_t nack = I2C_BB_SDA_READ();
    I2C_BB_SCL_LOW();

    return nack;
}

uint8_t I2C_BB_RecvByte(uint8_t ack)
{
    uint8_t byte = 0;

    I2C_BB_SDA_HIGH();
    for(uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;
        I2C_BB_SCL_HIGH();
        I2C_BB_Delay();
        if(I2C_BB_SDA_READ())
            byte |= 1;
        I2C_BB_SCL_LOW();
        I2C_BB_Delay();
    }

    if(ack)
        I2C_BB_SDA_LOW();
    else
        I2C_BB_SDA_HIGH();
    I2C_BB_Delay();
    I2C_BB_SCL_HIGH();
    I2C_BB_Delay();
    I2C_BB_SCL_LOW();
    I2C_BB_SDA_HIGH();

    return byte;
}

uint8_t I2C_CheckAndRecoverBus(void)
{
    uint8_t timeout = 9;

    I2C_BB_SDA_HIGH();
    I2C_BB_SCL_HIGH();
    I2C_BB_Delay();

    while(timeout--)
    {
        I2C_BB_SCL_LOW();
        I2C_BB_Delay();
        I2C_BB_SCL_HIGH();
        I2C_BB_Delay();
    }

    I2C_BB_Stop();
    return 0;
}
