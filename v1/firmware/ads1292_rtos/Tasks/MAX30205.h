/*****************************************************************
   Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : MAX30205.h
  Description : MAX30205高精度温度传感器驱动库头文件，定义了寄存器地址、数据结构、配置参数及API函数
  History     :
      euin   [2026-03-23]   create
*****************************************************************/



/* ================================= 头文件保护宏 ================================= */
#ifndef __MAX30205_H
#define __MAX30205_H



/* ================================= 依赖头文件包含区 ================================= */
#include "stm32f10x.h"
#include "TEMP_CONFIG.h"



/* ================================= 导出宏定义区 ================================= */

/************************** 调试信息配置 **************************/
/**
 * @brief 调试开关宏定义
 * @note  1=开启调试信息输出，0=关闭调试信息输出
 */
#ifndef MAX30205_DEBUG_ENABLE
#define MAX30205_DEBUG_ENABLE    1
#endif

#if MAX30205_DEBUG_ENABLE
#include <stdio.h>
#define MAX30205_DEBUG_PRINT(...)    printf(__VA_ARGS__)
#else
#define MAX30205_DEBUG_PRINT(...)
#endif

/**
 * @brief MAX30205 I2C设备地址（7位地址）
 * @note  当A0=A1=A2=GND时，地址为0x48；若地址线接法不同，需相应修改
 */
#define MAX30205_ADDR_7BIT      0x48

/**
 * @brief I2C写地址（8位格式：7位地址+1位写位）
 * @note  值为0x90
 */
#define MAX30205_ADDR_WRITE     (MAX30205_ADDR_7BIT << 1)

/**
 * @brief I2C读地址（8位格式：7位地址+1位读位）
 * @note  值为0x91
 */
#define MAX30205_ADDR_READ      ((MAX30205_ADDR_7BIT << 1) | 1)

/**
 * @brief 温度数据寄存器地址
 * @note  16位只读寄存器，存储温度测量值
 */
#define MAX30205_REG_TEMP       0x00

/**
 * @brief 配置寄存器地址
 * @note  用于配置传感器工作模式、分辨率等参数
 */
#define MAX30205_REG_CONFIG     0x01

/**
 * @brief 温度迟滞寄存器地址
 * @note  用于设置比较器模式的迟滞温度值
 */
#define MAX30205_REG_THYST      0x02

/**
 * @brief 温度上限寄存器地址
 * @note  用于设置比较器模式的过热报警阈值
 */
#define MAX30205_REG_TOS        0x03

/**
 * @brief 分辨率位掩码
 * @note  对应配置寄存器的D7-D5位 (R2-R0)
 */
#define MAX30205_CONFIG_RESOLUTION_MASK   0xE0

/**
 * @brief 16位分辨率配置值
 * @note  对应二进制：100
 */
#define MAX30205_CONFIG_RES_16BIT         0x80

/**
 * @brief 14位分辨率配置值
 * @note  对应二进制：011
 */
#define MAX30205_CONFIG_RES_14BIT         0x60

/**
 * @brief 12位分辨率配置值
 * @note  对应二进制：010
 */
#define MAX30205_CONFIG_RES_12BIT         0x40

/**
 * @brief 10位分辨率配置值
 * @note  对应二进制：001
 */
#define MAX30205_CONFIG_RES_10BIT         0x20

/**
 * @brief 8位分辨率配置值
 * @note  对应二进制：000
 */
#define MAX30205_CONFIG_RES_8BIT          0x00

/**
 * @brief 关断模式配置位
 * @note  设置此位可使传感器进入低功耗关断模式
 */
#define MAX30205_CONFIG_SD                0x04

/**
 * @brief 连续转换模式配置值
 * @note  传感器持续进行温度测量
 */
#define MAX30205_CONFIG_CONTINUOUS        0x00

/**
 * @brief 温度转换系数
 * @note  1 LSB = 0.00390625 ℃ (1/256 ℃)
 */
#define MAX30205_TEMP_LSB       0.00390625f



/* ================================= 导出类型定义区 ================================= */

/**
 * @brief MAX30205配置寄存器联合体定义
 * @note  支持按字节访问或按位访问配置寄存器
 */
typedef union {
    uint8_t reg;                          /**< 寄存器原始值 */
    struct {
        uint8_t shutdown    : 1;          /**< 关断模式位：1=关断，0=正常工作 */
        uint8_t comp_int    : 1;          /**< 比较器/中断模式位：1=中断模式，0=比较器模式 */
        uint8_t os_polarity : 1;          /**< OS极性位：1=高电平有效，0=低电平有效 */
        uint8_t fault_queue : 2;          /**< 故障队列位：定义触发报警所需的连续故障次数 */
        uint8_t data_format : 1;          /**< 数据格式位：1=扩展模式，0=正常模式 */
        uint8_t timeout     : 1;          /**< 超时使能位：1=使能超时功能，0=禁用 */
        uint8_t one_shot    : 1;          /**< 单次转换模式位：1=启动单次转换，0=连续转换 */
    } bits;
} max30205_config_reg_t;



/* ================================= 导出常量定义区 ================================= */
/* 温度转换系数已在宏定义区定义 */



/* ================================= 导出全局变量声明区 ================================= */



/* ================================= 导出函数声明区 ================================= */

/**
 * @brief  初始化MAX30205温度传感器
 * @note   配置传感器为连续转换模式，设置合适的分辨率
 * @retval uint8_t: 初始化结果，0表示失败，1表示成功
 */
uint8_t MAX30205_Init(void);

/**
 * @brief  读取当前温度值
 * @note   读取16位温度数据并转换为摄氏度浮点值
 * @retval float: 当前温度值（摄氏度）
 */
float MAX30205_ReadTemp(void);

/**
 * @brief  向MAX30205的指定寄存器写入数据
 * @param  regAddr: 寄存器地址
 * @param  data: 要写入的数据
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_WriteReg(uint8_t regAddr, uint8_t data);

/**
 * @brief  从MAX30205的指定寄存器读取数据
 * @param  regAddr: 寄存器地址
 * @param  data: 存储读取到的数据的指针
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_ReadReg(uint8_t regAddr, uint8_t *data);

/**
 * @brief  从MAX30205的指定寄存器开始连续读取多个字节
 * @param  regAddr: 起始寄存器地址
 * @param  buffer: 存储读取数据的缓冲区
 * @param  len: 要读取的字节数
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_ReadRegs(uint8_t regAddr, uint8_t *buffer, uint8_t len);

/**
 * @brief  读取MAX30205的配置寄存器值
 * @param  config: 存储配置寄存器值的指针
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_ReadConfiguration(max30205_config_reg_t *config);

/**
 * @brief  设置MAX30205的配置寄存器值
 * @param  config: 配置寄存器值
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_WriteConfiguration(max30205_config_reg_t config);

/**
 * @brief  读取MAX30205的温度迟滞寄存器值
 * @retval uint16_t: 温度迟滞寄存器值
 */
uint16_t MAX30205_ReadTHYST(void);

/**
 * @brief  设置MAX30205的温度迟滞寄存器值
 * @param  value: 要写入的值
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_WriteTHYST(uint16_t value);

/**
 * @brief  读取MAX30205的温度上限寄存器值
 * @retval uint16_t: 温度上限寄存器值
 */
uint16_t MAX30205_ReadTOS(void);

/**
 * @brief  设置MAX30205的温度上限寄存器值
 * @param  value: 要写入的值
 * @retval uint8_t: 操作结果，0表示失败，1表示成功
 */
uint8_t MAX30205_WriteTOS(uint16_t value);

/**
 * @brief  将原始温度数据转换为摄氏度
 * @param  rawTemp: 原始温度数据（16位）
 * @retval float: 摄氏度温度值
 */
float MAX30205_ToCelsius(uint16_t rawTemp);

/**
 * @brief  将摄氏度转换为华氏度
 * @param  temperatureC: 摄氏度温度值
 * @retval float: 华氏度温度值
 */
float MAX30205_ToFahrenheit(float temperatureC);

#endif /* __MAX30205_H */