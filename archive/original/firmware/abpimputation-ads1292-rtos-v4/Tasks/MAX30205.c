/*****************************************************************
  Copyright (c) [2026], [PhysioCard Team] All rights reserved
  File Name   : MAX30205.c
  Description : MAX30205高精度温度传感器驱动库实现文件，包含I2C通信、寄存器配置和温度转换等功能
  History     :
      PhysioCard Team   [2026-03-23]   create
      PhysioCard Team   [2026-03-24]   添加完整注释和格式优化
*****************************************************************/


/* ================================= 依赖头文件包含区 ================================= */

#include "MAX30205.h"
#include "filters.h"  


/* ================================= 全局变量定义区 ================================= */

// 此处定义本文件使用的全局变量（如果有的话）


/* ================================= 函数声明区 ================================= */

// 静态函数声明（如果有的话）


/* ================================= 函数实现区 ================================= */



/**
  * @brief  向 MAX30205 写入一个字节数据
  * @param  regAddr: 寄存器地址
  * @param  data: 要写入的数据
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_WriteReg(uint8_t regAddr, uint8_t data)
{
    MAX30205_DEBUG_PRINT("[MAX30205] Write Reg: Addr=0x%02X, Data=0x%02X\r\n", regAddr, data);

    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_WRITE)) { I2C_BB_Stop(); return 1; }
    if(I2C_BB_SendByte(regAddr)) { I2C_BB_Stop(); return 1; }
    if(I2C_BB_SendByte(data)) { I2C_BB_Stop(); return 1; }
    I2C_BB_Stop();

    MAX30205_DEBUG_PRINT("[MAX30205] Write Success\r\n");
    return 0;
}

/**
  * @brief  从 MAX30205 读取一个字节数据
  * @param  regAddr: 寄存器地址
  * @param  data: 读取数据缓冲区
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_ReadReg(uint8_t regAddr, uint8_t *data)
{
    MAX30205_DEBUG_PRINT("[MAX30205] Read Reg: Addr=0x%02X\r\n", regAddr);

    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_WRITE)) { I2C_BB_Stop(); return 1; }
    if(I2C_BB_SendByte(regAddr)) { I2C_BB_Stop(); return 1; }
    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_READ)) { I2C_BB_Stop(); return 1; }
    *data = I2C_BB_RecvByte(0);
    I2C_BB_Stop();

    MAX30205_DEBUG_PRINT("[MAX30205] Read Success: Data=0x%02X\r\n", *data);
    return 0;
}

/**
  * @brief  从 MAX30205 连续读取多个字节
  * @param  regAddr: 起始寄存器地址
  * @param  buffer: 数据缓冲区
  * @param  len: 读取长度（字节）
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_ReadRegs(uint8_t regAddr, uint8_t *buffer, uint8_t len)
{
    if(len == 0) return 0;

    MAX30205_DEBUG_PRINT("[MAX30205] Read Multiple Regs: StartAddr=0x%02X, Len=%d\r\n", regAddr, len);

    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_WRITE)) { I2C_BB_Stop(); return 1; }
    if(I2C_BB_SendByte(regAddr)) { I2C_BB_Stop(); return 1; }
    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_READ)) { I2C_BB_Stop(); return 1; }
    for(uint8_t i = 0; i < len; i++)
    {
        buffer[i] = I2C_BB_RecvByte(i < len - 1 ? 1 : 0);
    }
    I2C_BB_Stop();

    return 0;
}

/**
  * @brief  初始化 MAX30205 传感器
  * @param  无
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_Init(void)
{
    uint8_t config;
    
    MAX30205_DEBUG_PRINT("[MAX30205] Initializing sensor...\r\n");
    
    /* 检测设备是否在线 */
    if(MAX30205_ReadReg(MAX30205_REG_CONFIG, &config) != 0)
    {
        MAX30205_DEBUG_PRINT("[MAX30205] Device not detected!\r\n");
        return 1;
    }
    
    MAX30205_DEBUG_PRINT("[MAX30205] Current CONFIG: 0x%02X (Resolution bits: %d)\r\n", 
                         config, (config >> 5) & 0x07);
    
    /* 配置传感器：连续转换模式，16位分辨率 */
    uint8_t desired_config = MAX30205_CONFIG_RES_16BIT | MAX30205_CONFIG_CONTINUOUS;
    MAX30205_DEBUG_PRINT("[MAX30205] Writing CONFIG: 0x%02X (expecting resolution=4 for 16-bit)\r\n", desired_config);
    
    if(MAX30205_WriteReg(MAX30205_REG_CONFIG, desired_config) != 0)
    {
        MAX30205_DEBUG_PRINT("[MAX30205] Config write failed!\r\n");
        return 1;
    }
    
    /* 验证配置 */
    for(volatile uint32_t i = 0; i < 1000; i++);
    
    if(MAX30205_ReadReg(MAX30205_REG_CONFIG, &config) == 0)
    {
        MAX30205_DEBUG_PRINT("[MAX30205] CONFIG after write: 0x%02X (Resolution bits: %d)\r\n", 
                             config, (config >> 5) & 0x07);
        
        // 验证配置是否成功写入
        if((config & MAX30205_CONFIG_RESOLUTION_MASK) != MAX30205_CONFIG_RES_16BIT)
        {
            MAX30205_DEBUG_PRINT("[MAX30205] WARNING: Config write verification failed. Expected 0x%02X, got 0x%02X\r\n", 
                                 (desired_config & MAX30205_CONFIG_RESOLUTION_MASK), 
                                 (config & MAX30205_CONFIG_RESOLUTION_MASK));
        }
    }
    
    
    MAX30205_DEBUG_PRINT("[MAX30205] Init success!\r\n");
    return 0;
}

/**
  * @brief  读取配置寄存器
  * @param  config: 配置寄存器结构体指针
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_ReadConfiguration(max30205_config_reg_t *config)
{
    uint8_t reg_value;
    
    if(MAX30205_ReadReg(MAX30205_REG_CONFIG, &reg_value) != 0)
    {
        return 1;
    }
    
    config->reg = reg_value;
    return 0;
}

/**
  * @brief  写入配置寄存器
  * @param  config: 配置寄存器结构体
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_WriteConfiguration(max30205_config_reg_t config)
{
    return MAX30205_WriteReg(MAX30205_REG_CONFIG, config.reg);
}

/**
  * @brief  读取 THYST 寄存器
  * @retval THYST 寄存器的值，失败返回 0
  */
uint16_t MAX30205_ReadTHYST(void)
{
    uint8_t tempData[2];
    if(MAX30205_ReadRegs(MAX30205_REG_THYST, tempData, 2) != 0)
    {
        return 0;
    }
    // THYST 寄存器是 16 位值，高位在前
    return ((uint16_t)tempData[0] << 8) | tempData[1];
}

/**
  * @brief  写入 THYST 寄存器
  * @param  value: 要写入的值
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_WriteTHYST(uint16_t value)
{
    uint8_t tempData[2];
    tempData[0] = (uint8_t)(value >> 8);  // 高字节
    tempData[1] = (uint8_t)value;         // 低字节
    
    // 连续写入两个字节
    if(MAX30205_WriteReg(MAX30205_REG_THYST, tempData[0]) != 0)
        return 1;
    
    return MAX30205_WriteReg(MAX30205_REG_THYST + 1, tempData[1]);
}

/**
  * @brief  读取 TOS 寄存器
  * @retval TOS 寄存器的值，失败返回 0
  */
uint16_t MAX30205_ReadTOS(void)
{
    uint8_t tempData[2];
    if(MAX30205_ReadRegs(MAX30205_REG_TOS, tempData, 2) != 0)
    {
        return 0;
    }
    // TOS 寄存器是 16 位值，高位在前
    return ((uint16_t)tempData[0] << 8) | tempData[1];
}

/**
  * @brief  写入 TOS 寄存器
  * @param  value: 要写入的值
  * @retval 0:成功 1:失败
  */
uint8_t MAX30205_WriteTOS(uint16_t value)
{
    uint8_t tempData[2];
    tempData[0] = (uint8_t)(value >> 8);  // 高字节
    tempData[1] = (uint8_t)value;         // 低字节
    
    // 连续写入两个字节
    if(MAX30205_WriteReg(MAX30205_REG_TOS, tempData[0]) != 0)
        return 1;
    
    return MAX30205_WriteReg(MAX30205_REG_TOS + 1, tempData[1]);
}

/**
  * @brief  将原始温度值转换为摄氏度
  * @param  rawTemp: 原始温度值（16 位）
  * @retval 摄氏度温度值
  */
float MAX30205_ToCelsius(uint16_t rawTemp)
{
    // MAX30205 是 16 位精度，使用符号位表示负温度
    int16_t signedTemp = (int16_t)rawTemp;
    return (float)signedTemp * MAX30205_TEMP_LSB;
}

/**
  * @brief  将摄氏度转换为华氏度
  * @param  temperatureC: 摄氏度温度值
  * @retval 华氏度温度值
  */
float MAX30205_ToFahrenheit(float temperatureC)
{
    return (temperatureC * 9.0f / 5.0f) + 32.0f;
}

/**
  * @brief  读取温度值
  * @param  无
  * @retval 温度值（摄氏度，float 类型）
  */
float MAX30205_ReadTemp(void)
{
    uint8_t tempHigh, tempLow;
    uint16_t tempRaw;

    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_WRITE)) { I2C_BB_Stop(); return -999.0f; }
    if(I2C_BB_SendByte(MAX30205_REG_TEMP)) { I2C_BB_Stop(); return -999.0f; }
    I2C_BB_Start();
    if(I2C_BB_SendByte(MAX30205_ADDR_READ)) { I2C_BB_Stop(); return -999.0f; }
    tempHigh = I2C_BB_RecvByte(1);
    tempLow = I2C_BB_RecvByte(0);
    I2C_BB_Stop();

    tempRaw = ((uint16_t)tempHigh << 8) | tempLow;
    return (float)((int16_t)tempRaw) * MAX30205_TEMP_LSB;
}