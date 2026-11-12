/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : ECG.h
  Description : ADS1292R 心电采集芯片驱动头文件
                包含寄存器定义、配置结构体、GPIO宏、函数声明
  History     :
      euin   2026-05-18   port from F1/Hardware/ADS1292.H
*****************************************************************/

#ifndef __ECG_H
#define __ECG_H

#include "stm32f10x.h"
#include <stdint.h>

/* ================================ 演示模式开关 ================================ */
// 取消注释下面这行启用演示模式，注释掉则使用实时模式
// #define ECG_DEMO_MODE   /* v1: 关闭演示回放，改用实时采集 */

/* ================================ GPIO 宏定义区 ================================ */
/** 替代 sys.h 的 PAin/PAout/PBout 宏 */

/** @brief 读取 DRDY 引脚状态 (PA1) */
#define ADS_DRDY_READ()     GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)

/** @brief CS 片选引脚控制 (PA4) */
#define ADS_CS_HIGH()       GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define ADS_CS_LOW()        GPIO_ResetBits(GPIOA, GPIO_Pin_4)

/** @brief RESET 复位引脚控制 (PB0) */
#define ADS_RESET_HIGH()    GPIO_SetBits(GPIOB, GPIO_Pin_0)
#define ADS_RESET_LOW()     GPIO_ResetBits(GPIOB, GPIO_Pin_0)

/** @brief START 启动引脚控制 (PB1) */
#define ADS_START_HIGH()    GPIO_SetBits(GPIOB, GPIO_Pin_1)
#define ADS_START_LOW()     GPIO_ResetBits(GPIOB, GPIO_Pin_1)

/* ================================ 外部变量声明区 ================================ */

/** @brief ADS1292R 数据缓存（9字节：状态+2通道×24bit） */
extern volatile uint8_t g_ads1292_cache[9];

/** @brief 数据读取完成标志 */
extern volatile uint8_t g_ads1292_recv_flag;

/* ================================ ADS1292R 命令定义区 ================================ */

/** @brief 系统命令 */
#define WAKEUP      0x02    /**< 从待机模式唤醒 */
#define STANDBY     0x04    /**< 进入待机模式 */
#define ADS_RESET   0x06    /**< 复位ADS1292R */
#define START_CMD   0x08    /**< 开始数据转换 */
#define STOP        0x0A    /**< 停止转换 */
#define OFFSETCAL   0x1A    /**< 通道偏移校准 */

/** @brief 数据读取命令 */
#define RDATAC      0x10    /**< 启动连续数据读取模式（默认） */
#define SDATAC      0x11    /**< 停止连续数据读取模式 */
#define RDATA       0x12    /**< 通过命令读取数据（单次读取） */

/** @brief 寄存器读写命令 */
#define RREG        0x20    /**< 读寄存器: 001r rrrr(1字节) 000n nnnn(2字节) */
#define WREG        0x40    /**< 写寄存器: 010r rrrr(1字节) 000n nnnn(2字节) */

/* ================================ 寄存器地址定义区 ================================ */

#define REG_ID          0   /**< ID 寄存器（只读） */
#define REG_CONFIG1     1   /**< 配置寄存器1 */
#define REG_CONFIG2     2   /**< 配置寄存器2 */
#define REG_LOFF        3   /**< 导联脱落控制寄存器 */
#define REG_CH1SET      4   /**< 通道1配置寄存器 */
#define REG_CH2SET      5   /**< 通道2配置寄存器 */
#define REG_RLD_SENS    6   /**< 右腿驱动选择寄存器 */
#define REG_LOFF_SENS   7   /**< 导联脱落检测选择寄存器 */
#define REG_LOFF_STAT   8   /**< 导联脱落状态寄存器（只读） */
#define REG_RESP1       9   /**< 呼吸测量控制寄存器1 */
#define REG_RESP2       10  /**< 呼吸测量控制寄存器2 */
#define REG_GPIO        11  /**< GPIO控制寄存器 */

/* ================================ 寄存器字段值定义区 ================================ */

/** @brief ID 寄存器 */
#define DEVICE_ID_ADS1292       0x53
#define DEVICE_ID_ADS1292R      0x73

/** @brief CONFIG1 - 数据速率 */
#define DATA_RATE_125SPS        0x00
#define DATA_RATE_250SPS        0x01
#define DATA_RATE_500SPS        0x02
#define DATA_RATE_1kSPS         0x03
#define DATA_RATE_2kSPS         0x04
#define DATA_RATE_4kSPS         0x05
#define DATA_RATE_8kSPS         0x06

/** @brief CONFIG2 - 导联脱落比较器 */
#define PDB_LOFF_COMP_OFF       0
#define PDB_LOFF_COMP_ON        1

/** @brief CONFIG2 - 基准电压缓冲 */
#define PDB_REFBUF_OFF          0
#define PDB_REFBUF_ON           1

/** @brief CONFIG2 - 内部基准电压选择 */
#define VREF_242V               0   /**< 内部基准电压2.42V（默认） */
#define VREF_4V                 1   /**< 内部基准电压4.033V */

/** @brief CONFIG2 - 时钟输出使能 */
#define CLK_EN_OFF              0
#define CLK_EN_ON               1

/** @brief CONFIG2 - 内部测试信号 */
#define INT_TEST_OFF            0
#define INT_TEST_ON             1

/** @brief CHSET - 通道电源 */
#define PD_ON                   0   /**< 通道正常工作中（默认） */
#define PD_OFF                  1   /**< 通道掉电 */

/** @brief CHSET - PGA 增益 */
#define GAIN_6                  0   /**< 增益6（默认） */
#define GAIN_1                  1
#define GAIN_2                  2
#define GAIN_3                  3
#define GAIN_4                  4
#define GAIN_8                  5
#define GAIN_12                 6

/** @brief CHSET - 输入多路复用 */
#define MUX_Normal_input        0   /**< 正常电极输入（默认） */
#define MUX_input_shorted       1   /**< 输入短路 */
#define MUX_Test_signal         5   /**< 测试信号输入 */
#define MUX_RLD_DRP             6
#define MUX_RLD_DRM             7
#define MUX_RLD_DRPM            8
#define MUX_RSP_IN3P            9   /**< 呼吸通道1 */

/** @brief RLD_SENS */
#define PDB_RLD_OFF             0
#define PDB_RLD_ON              1
#define RLD_LOFF_SENSE_OFF      0
#define RLD_LOFF_SENSE_ON       1
#define RLD_CANNLE_OFF          0
#define RLD_CANNLE_ON           1

/** @brief LOFF_SENS */
#define FLIP2_OFF               0
#define FLIP2_ON                1
#define FLIP1_OFF               0
#define FLIP1_ON                1
#define LOFF_CANNLE_OFF         0
#define LOFF_CANNLE_ON          1

/** @brief RESP1 */
#define RESP_DEMOD_OFF          0
#define RESP_DEMOD_ON           1
#define RESP_MOD_OFF            0
#define RESP_MOD_ON             1
#define RESP_CTRL_CLOCK_INTERNAL    0
#define RESP_CTRL_CLOCK_EXTERNAL    1

/** @brief RESP2 */
#define CALIB_OFF               0
#define CALIB_ON                1
#define FREQ_32K                0   /**< 默认 */
#define FREQ_64K                1
#define RLDREF_INT_EXTERN       0   /**< 外部提供RLDREF */
#define RLDREF_INT_INTERNALLY   1   /**< 内部生成 */

/* ================================ 寄存器默认配置定义区 ================================ */

/** @brief ADS1292R 系列芯片和通道相关默认配置 */
#define ADS1292_DEVICE          DEVICE_ID_ADS1292R
#define DATA_RATE               DATA_RATE_250SPS
#define PDB_LOFF_COMP_CFG       PDB_LOFF_COMP_ON
#define PDB_REFBUF_CFG          PDB_REFBUF_ON
#define VREF_CFG                VREF_242V
#define CLK_EN_CFG              CLK_EN_OFF
#define INT_TEST_CFG            INT_TEST_OFF
#define CH1_POWER               PD_ON
#define CH1_GAIN                GAIN_6
#define CH1_MUX                 MUX_Normal_input
#define CH2_POWER               PD_ON
#define CH2_GAIN                GAIN_6
#define CH2_MUX                 MUX_Normal_input
#define PDB_RLD_CFG             PDB_RLD_ON
#define RLD_LOFF_SENSE_CFG      RLD_LOFF_SENSE_OFF
#define RLD2N_CFG               RLD_CANNLE_ON
#define RLD2P_CFG               RLD_CANNLE_ON
#define RLD1N_CFG               RLD_CANNLE_OFF
#define RLD1P_CFG               RLD_CANNLE_OFF
#define FLIP2_CFG               FLIP2_OFF
#define FLIP1_CFG               FLIP1_OFF
#define LOFF2N_CFG              RLD_CANNLE_ON
#define LOFF2P_CFG              RLD_CANNLE_ON
#define LOFF1N_CFG              RLD_CANNLE_OFF
#define LOFF1P_CFG              RLD_CANNLE_OFF
#define RESP_DEMOD_EN1_CFG      RESP_DEMOD_OFF
#define RESP_MOD_EN_CFG         RESP_MOD_OFF
#define RESP_PH_CFG             0x0D
#define RESP_CTRL_CFG           RESP_CTRL_CLOCK_INTERNAL
#define CALIB_CFG               CALIB_OFF
#define FREQ_CFG                FREQ_32K
#define RLDREF_INT_CFG          RLDREF_INT_INTERNALLY

/* ================================ 配置结构体定义区 ================================ */

/** @brief CONFIG1 寄存器配置结构体 */
typedef struct {
    uint8_t Data_Rate;      /**< ADC通道数据速率 */
} ADS1292_CONFIG1;

/** @brief CONFIG2 寄存器配置结构体 */
typedef struct {
    uint8_t Pdb_Loff_Comp;  /**< 导联脱落比较器是否使能 */
    uint8_t Pdb_Refbuf;     /**< 内部基准缓冲是否使能 */
    uint8_t Vref;           /**< 内部基准电压选择 */
    uint8_t Clk_EN;         /**< 时钟输出使能控制 */
    uint8_t Int_Test;       /**< 内部测试信号使能位 */
} ADS1292_CONFIG2;

/** @brief CHSET 寄存器配置结构体 */
typedef struct {
    uint8_t PD;             /**< 通道电源控制 */
    uint8_t GAIN;           /**< PGA 增益设置 */
    uint8_t MUX;            /**< 输入通道多路复用方式 */
} ADS1292_CHSET;

/** @brief RLD_SENS 寄存器配置结构体 */
typedef struct {
    uint8_t Pdb_Rld;        /**< 右腿驱动电源状态 */
    uint8_t Rld_Loff_Sense; /**< RLD 导联脱落检测功能 */
    uint8_t Rld2N;          /**< 通道2负端 RLD 选择 */
    uint8_t Rld2P;          /**< 通道2正端 RLD 选择 */
    uint8_t Rld1N;          /**< 通道1负端 RLD 选择 */
    uint8_t Rld1P;          /**< 通道1正端 RLD 选择 */
} ADS1292_RLD_SENS;

/** @brief LOFF_SENS 寄存器配置结构体 */
typedef struct {
    uint8_t Flip2;          /**< 控制电激励通道2的电流方向 */
    uint8_t Flip1;          /**< 控制电激励通道1的电流方向 */
    uint8_t Loff2N;         /**< 通道2负端导联脱落检测 */
    uint8_t Loff2P;         /**< 通道2正端导联脱落检测 */
    uint8_t Loff1N;         /**< 通道1负端导联脱落检测 */
    uint8_t Loff1P;         /**< 通道1正端导联脱落检测 */
} ADS1292_LOFF_SENS;

/** @brief RESP1 寄存器配置结构体 */
typedef struct {
    uint8_t RESP_DemodEN;   /**< 通道1解调电路控制 */
    uint8_t RESP_modEN;     /**< 通道1调制电路控制 */
    uint8_t RESP_ph;        /**< 呼吸信号相位 */
    uint8_t RESP_Ctrl;      /**< 呼吸电路模式控制 */
} ADS1292_RESP1;

/** @brief RESP2 寄存器配置结构体 */
typedef struct {
    uint8_t Calib;          /**< 通道偏移校准控制 */
    uint8_t freq;           /**< 呼吸调制频率 32K/64K */
    uint8_t Rldref_Int;     /**< RLDREF 信号源选择 */
} ADS1292_RESP2;

/* ================================ 函数声明区 ================================ */

/**
 * @brief  ADS1292R 硬件初始化（GPIO + SPI1 + EXTI1 中断）
 * @retval 无
 */
void ADS1292_Init(void);

/**
 * @brief  ADS1292R 上电初始化（复位、停止连续读取、读取默认寄存器）
 * @retval 无
 */
void ADS1292_PowerOnInit(void);

/**
 * @brief  SPI1 发送/接收一个字节
 * @param  tx_byte 要发送的字节
 * @retval 接收到的字节
 */
uint8_t ADS1292_SPI(uint8_t tx_byte);

/**
 * @brief  向 ADS1292R 发送单字节命令
 * @param  cmd 命令字节
 * @retval 无
 */
void ADS1292_Send_CMD(uint8_t cmd);

/**
 * @brief  读写 ADS1292R 连续寄存器
 * @param  reg  寄存器地址（含 RREG/WREG 前缀）
 * @param  len  寄存器数量
 * @param  data 数据指针（读时为输出，写时为输入）
 * @retval 无
 */
void ADS1292_WR_REGS(uint8_t reg, uint8_t len, uint8_t *data);

/**
 * @brief  读取 ADS1292R 9字节数据（状态+2通道×24bit）
 * @param  data 9字节缓冲区指针
 * @retval 始终返回0
 */
uint8_t ADS1292_Read_Data(uint8_t *data);

/**
 * @brief  设置寄存器缓冲区（根据结构体配置生成寄存器值）
 * @retval 无
 */
void ADS1292_SET_REGBUFF(void);

/**
 * @brief  将寄存器缓冲区写入芯片并校验
 * @retval 0=成功, 1=寄存器校验失败
 */
uint8_t ADS1292_WRITE_REGBUFF(void);

/**
 * @brief  内部噪声测试（输入短路）
 * @retval 0=成功, 1=寄存器写入失败
 */
uint8_t ADS1292_Noise_Test(void);

/**
 * @brief  通道1内部1mV方波测试信号
 * @retval 0=成功, 1=寄存器写入失败
 */
uint8_t ADS1292_Single_Test(void);

/**
 * @brief  正常信号采集模式
 * @retval 0=成功, 1=寄存器写入失败
 */
uint8_t ADS1292_Single_Read(void);

/**
 * @brief  设置 ADS1292R 采集模式并启动转换
 * @param  mode 0=正常采集, 1=内部测试信号, 2=噪声测试
 * @retval 0=成功, 1=失败
 */
uint8_t Set_ADS1292_Collect(uint8_t mode);

/**
 * @brief  ECG 采集任务（FreeRTOS 任务函数）
 * @param  argument 未使用
 * @retval 无
 */
void ECG_Task(void *argument);

#endif /* __ECG_H */
