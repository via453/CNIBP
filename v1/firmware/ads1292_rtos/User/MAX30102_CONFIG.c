#include "MAX30102_CONFIG.h"

/**
 * @brief MAX30102 I2C GPIO初始化 (PB6=SCL, PB7=SDA, PB8=INT)
 */
void MAX30102_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(MAX30102_SCL_GPIO_CLK | MAX30102_SDA_GPIO_CLK, ENABLE);

    /* SCL - 开漏输出 */
    GPIO_InitStructure.GPIO_Pin = MAX30102_SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MAX30102_SCL_GPIO_PORT, &GPIO_InitStructure);

    /* SDA - 开漏输出 */
    GPIO_InitStructure.GPIO_Pin = MAX30102_SDA_PIN;
    GPIO_Init(MAX30102_SDA_GPIO_PORT, &GPIO_InitStructure);

    MAX30102_I2C_SCL_HIGH();
    MAX30102_I2C_SDA_HIGH();
}

/**
 * @brief MAX30102 INT引脚初始化 (PB8, 上拉输入)
 */
void MAX30102_INT_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(MAX30102_INT_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin = MAX30102_INT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(MAX30102_INT_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief I2C软件延时
 */
void MAX30102_I2C_Delay(void)
{
    for(volatile uint32_t i = 0; i < 120; i++);
}

/**
 * @brief I2C起始信号
 */
void MAX30102_I2C_Start(void)
{
    MAX30102_I2C_SDA_HIGH();
    MAX30102_I2C_SCL_HIGH();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SDA_LOW();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SCL_LOW();
}

/**
 * @brief I2C停止信号
 */
void MAX30102_I2C_Stop(void)
{
    MAX30102_I2C_SDA_LOW();
    MAX30102_I2C_SCL_HIGH();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SDA_HIGH();
    MAX30102_I2C_Delay();
}

/**
 * @brief I2C发送一个字节
 * @param byte 要发送的字节
 * @return 0=收到ACK, 1=收到NACK
 */
uint8_t MAX30102_I2C_SendByte(uint8_t byte)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        if(byte & 0x80)
            MAX30102_I2C_SDA_HIGH();
        else
            MAX30102_I2C_SDA_LOW();
        MAX30102_I2C_Delay();
        MAX30102_I2C_SCL_HIGH();
        MAX30102_I2C_Delay();
        MAX30102_I2C_SCL_LOW();
        byte <<= 1;
    }

    MAX30102_I2C_SDA_HIGH();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SCL_HIGH();
    MAX30102_I2C_Delay();
    uint8_t nack = MAX30102_I2C_SDA_READ();
    MAX30102_I2C_SCL_LOW();

    return nack;
}

/**
 * @brief I2C接收一个字节
 * @param ack 1=发送ACK, 0=发送NACK
 * @return 接收到的字节
 */
uint8_t MAX30102_I2C_RecvByte(uint8_t ack)
{
    uint8_t byte = 0;

    MAX30102_I2C_SDA_HIGH();
    for(uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;
        MAX30102_I2C_SCL_HIGH();
        MAX30102_I2C_Delay();
        if(MAX30102_I2C_SDA_READ())
            byte |= 1;
        MAX30102_I2C_SCL_LOW();
        MAX30102_I2C_Delay();
    }

    if(ack)
        MAX30102_I2C_SDA_LOW();
    else
        MAX30102_I2C_SDA_HIGH();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SCL_HIGH();
    MAX30102_I2C_Delay();
    MAX30102_I2C_SCL_LOW();
    MAX30102_I2C_SDA_HIGH();

    return byte;
}

/**
 * @brief I2C读取单个寄存器
 * @param device_addr 设备地址 (8位写地址, 如0xAE)
 * @param register_addr 寄存器地址
 * @return 寄存器值
 */
uint8_t MAX30102_I2C_ReadByte(uint8_t device_addr, uint8_t register_addr)
{
    uint8_t read_data;

    MAX30102_I2C_Start();
    MAX30102_I2C_SendByte(device_addr + 0);
    MAX30102_I2C_SendByte(register_addr);
    MAX30102_I2C_Start();
    MAX30102_I2C_SendByte(device_addr + 1);
    read_data = MAX30102_I2C_RecvByte(0);
    MAX30102_I2C_Stop();

    return read_data;
}

/**
 * @brief I2C写入单个寄存器
 * @param device_addr 设备地址 (8位写地址, 如0xAE)
 * @param register_addr 寄存器地址
 * @param data 要写入的数据
 * @return 1=成功, 0=失败
 */
uint8_t MAX30102_I2C_WriteByte(uint8_t device_addr, uint8_t register_addr, uint8_t data)
{
    MAX30102_I2C_Start();
    if(MAX30102_I2C_SendByte(device_addr + 0)) return 0;
    if(MAX30102_I2C_SendByte(register_addr)) return 0;
    if(MAX30102_I2C_SendByte(data)) return 0;
    MAX30102_I2C_Stop();

    return 1;
}

/**
 * @brief I2C读取多个字节 (用于FIFO数据读取)
 * @param device_addr 设备地址 (8位写地址, 如0xAE)
 * @param register_addr 起始寄存器地址
 * @param data 数据缓冲区指针
 * @param num 要读取的字节数
 * @return 1=成功, 0=失败
 */
uint8_t MAX30102_I2C_ReadArray(uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t num)
{
    uint16_t i;

    MAX30102_I2C_Start();
    if(MAX30102_I2C_SendByte(device_addr + 0)) return 0;
    if(MAX30102_I2C_SendByte(register_addr)) return 0;
    MAX30102_I2C_Start();
    if(MAX30102_I2C_SendByte(device_addr + 1)) return 0;

    for(i = 0; i < num; i++)
    {
        data[i] = MAX30102_I2C_RecvByte((i == num - 1) ? 0 : 1);
    }

    MAX30102_I2C_Stop();

    return 1;
}
