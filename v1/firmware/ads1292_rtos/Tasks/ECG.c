/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : ECG.c
  Description : ADS1292R 心电采集芯片驱动实现
                包含 SPI1 通信、ADS1292R 初始化/配置/数据读取、EXTI1 中断处理
  History     :
      euin   2026-05-18   port from F1/Hardware/ADS1292.C + spi.c
*****************************************************************/

#include "ECG.h"
#include "Delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

/* ================================ 调试开关区 ================================ */

#ifdef DEBUG_OUTPUT
#define DEBUG_ADS1292   /**< 使能寄存器 printf 调试输出 */
#endif

/* ================================ 全局变量区 ================================ */

/** @brief ADS1292R 12字节寄存器缓冲区 */
uint8_t ADS1292_REG[12];

/** @brief 数据缓存和接收标志 */
volatile uint8_t g_ads1292_cache[9];
volatile uint8_t g_ads1292_recv_flag = 0;

/** @brief 寄存器配置结构体实例 */
ADS1292_CONFIG1  g_ads_cfg1  = { DATA_RATE };
ADS1292_CONFIG2  g_ads_cfg2  = {
    PDB_LOFF_COMP_CFG, PDB_REFBUF_CFG, VREF_CFG, CLK_EN_CFG, INT_TEST_CFG
};
ADS1292_CHSET    g_ads_ch1   = { CH1_POWER, CH1_GAIN, CH1_MUX };
ADS1292_CHSET    g_ads_ch2   = { CH2_POWER, CH2_GAIN, CH2_MUX };
ADS1292_RLD_SENS g_ads_rld   = {
    PDB_RLD_CFG, RLD_LOFF_SENSE_CFG, RLD2N_CFG, RLD2P_CFG, RLD1N_CFG, RLD1P_CFG
};
ADS1292_LOFF_SENS g_ads_loff = {
    FLIP2_CFG, FLIP1_CFG, LOFF2N_CFG, LOFF2P_CFG, LOFF1N_CFG, LOFF1P_CFG
};
ADS1292_RESP1    g_ads_resp1 = { RESP_DEMOD_EN1_CFG, RESP_MOD_EN_CFG, RESP_PH_CFG, RESP_CTRL_CFG };
ADS1292_RESP2    g_ads_resp2 = { CALIB_CFG, FREQ_CFG, RLDREF_INT_CFG };

/* ================================ SPI1 私有函数实现区 ================================ */

/**
 * @brief  SPI1 初始化（PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS）
 * @retval 无
 * @note   SPI Mode 1: CPOL=Low, CPHA=2Edge, MSB first, 4.5MHz (72M/16)
 */
static void SPI1_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef   SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* SCK=PA5, MISO=PA6, MOSI=PA7 — 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Direction      = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode           = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize       = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL           = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA           = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS            = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16; /* 72M/16 = 4.5MHz */
    SPI_InitStructure.SPI_FirstBit       = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial  = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

/**
 * @brief  SPI1 读写一个字节
 * @param  tx_byte 要发送的字节
 * @retval 接收到的字节
 */
static uint8_t SPI1_ReadWriteByte(uint8_t tx_byte)
{
    uint8_t retry = 0;

    /* 等待发送缓冲区空 */
    while ((SPI1->SR & (1 << 1)) == 0) {
        if (++retry > 200) return 0;
    }
    SPI1->DR = tx_byte;

    /* 等待接收缓冲区非空 */
    retry = 0;
    while ((SPI1->SR & (1 << 0)) == 0) {
        if (++retry > 200) return 0;
    }
    return SPI1->DR;
}

/* ================================ ADS1292R 硬件初始化区 ================================ */

/**
 * @brief  ADS1292R 硬件初始化（GPIO + SPI1 + EXTI1 中断）
 * @retval 无
 * @note   DRDY=PA1(输入上拉), CS=PA4(推挽), RESET=PB0(推挽), START=PB1(推挽)
 *         EXTI1 NVIC preemption priority = 12 (FreeRTOS Group_4 safe)
 */
void ADS1292_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE
    );

    SPI1_Init();

    /* DRDY — PA1 输入上拉（空闲高电平，数据就绪时低电平） */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* CS — PA4 推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* RESET — PB0 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* START — PB1 推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* DRDY 下降沿中断配置 */
    EXTI_ClearITPendingBit(EXTI_Line1);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource1);
    EXTI_InitStructure.EXTI_Line    = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* NVIC 配置：Group_4, preemption=12, sub=0 (FreeRTOS 安全) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    NVIC_InitStructure.NVIC_IRQChannel                   = EXTI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 初始化阶段禁止 EXTI1 中断（上电初始化期间不需要） */
    EXTI->IMR &= ~EXTI_Line1;
    ADS_CS_HIGH();

    ADS1292_PowerOnInit();
}

/* ================================ EXTI1 中断处理区 ================================ */

/**
 * @brief  EXTI1 中断服务函数（DRDY 下降沿触发）
 * @note   读取 9 字节数据并置位接收标志
 */
void EXTI1_IRQHandler(void)
{
    if ((EXTI->IMR & EXTI_Line1) && (ADS_DRDY_READ() == 0)) {
        EXTI_ClearITPendingBit(EXTI_Line1);
        ADS1292_Read_Data((uint8_t *)g_ads1292_cache);
        g_ads1292_recv_flag = 1;
    }
}

/* ================================ ADS1292R SPI 通信函数区 ================================ */

/**
 * @brief  ADS1292R SPI 读写一个字节
 * @param  tx_byte 要发送的字节
 * @retval 接收到的字节
 */
uint8_t ADS1292_SPI(uint8_t tx_byte)
{
    return SPI1_ReadWriteByte(tx_byte);
}

/**
 * @brief  向 ADS1292R 发送单字节命令
 * @param  cmd 命令字节
 * @retval 无
 */
void ADS1292_Send_CMD(uint8_t cmd)
{
    ADS_CS_LOW();
    Delay_us(100);
    ADS1292_SPI(cmd);
    Delay_us(100);
    ADS_CS_HIGH();
}

/**
 * @brief  读写 ADS1292R 连续寄存器
 * @param  reg  寄存器地址（含 RREG/WREG 前缀）
 * @param  len  寄存器数量
 * @param  data 数据指针（读时为输出，写时为输入）
 * @retval 无
 */
void ADS1292_WR_REGS(uint8_t reg, uint8_t len, uint8_t *data)
{
    uint8_t i;

    ADS_CS_LOW();
    Delay_us(100);
    ADS1292_SPI(reg);
    Delay_us(100);
    ADS1292_SPI(len - 1);

    if (reg & 0x40) {
        /* 写操作 */
        for (i = 0; i < len; i++) {
            Delay_us(100);
            ADS1292_SPI(*data);
            data++;
        }
    } else {
        /* 读操作 */
        for (i = 0; i < len; i++) {
            Delay_us(100);
            *data = ADS1292_SPI(0);
            data++;
        }
    }

    Delay_us(100);
    ADS_CS_HIGH();
}

/**
 * @brief  读取 ADS1292R 9字节数据
 * @param  data 9字节缓冲区指针
 * @retval 始终返回0
 */
uint8_t ADS1292_Read_Data(uint8_t *data)
{
    uint8_t i;

    ADS_CS_LOW();
    for (i = 0; i < 9; i++) {
        *data = ADS1292_SPI(0x00);
        data++;
    }
    ADS_CS_HIGH();

    return 0;
}

/* ================================ 寄存器配置函数区 ================================ */

/**
 * @brief  根据配置结构体生成寄存器缓冲区值
 * @retval 无
 */
void ADS1292_SET_REGBUFF(void)
{
    /* ID — 只读 */
    ADS1292_REG[REG_ID] = ADS1292_DEVICE;

    /* CONFIG1: [7]=0(连续转换) [6:3]=0(保留) [2:0]=数据速率 */
    ADS1292_REG[REG_CONFIG1]  = 0x00;
    ADS1292_REG[REG_CONFIG1] |= g_ads_cfg1.Data_Rate;

    /* CONFIG2: [7]=1(保留) [6]=导联脱落比较器 [5]=基准缓冲 [4]=基准电压 [3]=CLK [1]=测试信号 [0]=1(保留) */
    ADS1292_REG[REG_CONFIG2]  = 0x00;
    ADS1292_REG[REG_CONFIG2] |= (g_ads_cfg2.Pdb_Loff_Comp << 6);
    ADS1292_REG[REG_CONFIG2] |= (g_ads_cfg2.Pdb_Refbuf     << 5);
    ADS1292_REG[REG_CONFIG2] |= (g_ads_cfg2.Vref           << 4);
    ADS1292_REG[REG_CONFIG2] |= (g_ads_cfg2.Clk_EN         << 3);
    ADS1292_REG[REG_CONFIG2] |= (g_ads_cfg2.Int_Test       << 1);
    ADS1292_REG[REG_CONFIG2] |= 0x81; /* 设置默认保留位 */

    /* LOFF: [7:5]=比较器阈值 [4]=1(保留) [3:2]=电流幅值 [1]=0(保留) [0]=检测方式(0=DC) */
    ADS1292_REG[REG_LOFF] = 0x10;

    /* CH1SET: [7]=电源 [6:4]=增益 [3:0]=输入选择 */
    ADS1292_REG[REG_CH1SET]  = 0x00;
    ADS1292_REG[REG_CH1SET] |= (g_ads_ch1.PD   << 7);
    ADS1292_REG[REG_CH1SET] |= (g_ads_ch1.GAIN << 4);
    ADS1292_REG[REG_CH1SET] |=  g_ads_ch1.MUX;

    /* CH2SET */
    ADS1292_REG[REG_CH2SET]  = 0x00;
    ADS1292_REG[REG_CH2SET] |= (g_ads_ch2.PD   << 7);
    ADS1292_REG[REG_CH2SET] |= (g_ads_ch2.GAIN << 4);
    ADS1292_REG[REG_CH2SET] |=  g_ads_ch2.MUX;

    /* RLD_SENS: [7:6]=11(PGA终端频率fMOD/4) [5]=RLD电源 [4]=脱落检测 [3:0]=通道选择 */
    ADS1292_REG[REG_RLD_SENS]  = 0x00;
    ADS1292_REG[REG_RLD_SENS] |= (g_ads_rld.Pdb_Rld       << 5);
    ADS1292_REG[REG_RLD_SENS] |= (g_ads_rld.Rld_Loff_Sense << 4);
    ADS1292_REG[REG_RLD_SENS] |= (g_ads_rld.Rld2N          << 3);
    ADS1292_REG[REG_RLD_SENS] |= (g_ads_rld.Rld2P          << 2);
    ADS1292_REG[REG_RLD_SENS] |= (g_ads_rld.Rld1N          << 1);
    ADS1292_REG[REG_RLD_SENS] |=  g_ads_rld.Rld1P;
    ADS1292_REG[REG_RLD_SENS] |= 0xC0; /* 设置默认保留位 */

    /* LOFF_SENS: [7:6]=0(保留) [5]=Flip2 [4]=Flip1 [3:0]=通道脱落检测 */
    ADS1292_REG[REG_LOFF_SENS]  = 0x00;
    ADS1292_REG[REG_LOFF_SENS] |= (g_ads_loff.Flip2  << 5);
    ADS1292_REG[REG_LOFF_SENS] |= (g_ads_loff.Flip1  << 4);
    ADS1292_REG[REG_LOFF_SENS] |= (g_ads_loff.Loff2N << 3);
    ADS1292_REG[REG_LOFF_SENS] |= (g_ads_loff.Loff2P << 2);
    ADS1292_REG[REG_LOFF_SENS] |= (g_ads_loff.Loff1N << 1);
    ADS1292_REG[REG_LOFF_SENS] |=  g_ads_loff.Loff1P;

    /* LOFF_STAT: [6]=0(频率) [4:0]=只读（脱落状态） */
    ADS1292_REG[REG_LOFF_STAT] = 0x00;

    /* RESP1: [7]=解调使能 [6]=调制使能 [5:2]=相位 [0]=时钟模式 */
    ADS1292_REG[REG_RESP1]  = 0x00;
    ADS1292_REG[REG_RESP1] |= (g_ads_resp1.RESP_DemodEN << 7);
    ADS1292_REG[REG_RESP1] |= (g_ads_resp1.RESP_modEN   << 6);
    ADS1292_REG[REG_RESP1] |= (g_ads_resp1.RESP_ph      << 2);
    ADS1292_REG[REG_RESP1] |=  g_ads_resp1.RESP_Ctrl;
    ADS1292_REG[REG_RESP1] |= 0x02; /* 设置默认保留位 */

    /* RESP2: [7]=校准 [6:3]=0(保留) [2]=频率 [1]=RLDREF源 [0]=1(保留) */
    ADS1292_REG[REG_RESP2]  = 0x00;
    ADS1292_REG[REG_RESP2] |= (g_ads_resp2.Calib      << 7);
    ADS1292_REG[REG_RESP2] |= (g_ads_resp2.freq       << 2);
    ADS1292_REG[REG_RESP2] |= (g_ads_resp2.Rldref_Int << 1);
    ADS1292_REG[REG_RESP2] |= 0x01; /* 设置默认保留位 */

    /* GPIO: [7:4]=0(保留) [3:2]=11(GPIO为输入) [1:0]=输出电平 */
    ADS1292_REG[REG_GPIO] = 0x0C;
}

/**
 * @brief  将寄存器缓冲区写入芯片并校验
 * @retval 0=成功, 1=寄存器校验失败
 */
uint8_t ADS1292_WRITE_REGBUFF(void)
{
    uint8_t i, res = 0;
    uint8_t reg_cache[12];

    ADS1292_SET_REGBUFF();

    /* 写入 CONFIG1~GPIO（11个寄存器，从地址1开始） */
    ADS1292_WR_REGS(WREG | REG_CONFIG1, 11, ADS1292_REG + 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 读回全部12个寄存器 */
    ADS1292_WR_REGS(RREG | REG_ID, 12, reg_cache);
    vTaskDelay(pdMS_TO_TICKS(10));

#ifdef DEBUG_ADS1292
    printf("WRITE REG:\r\n");
    for (i = 0; i < 12; i++)
        printf("%d %x\r\n", i, ADS1292_REG[i]);
    printf("READ REG:\r\n");
#endif

    /* 校验：跳过 ID(0)、LOFF_STAT(8)、GPIO(11) 只读/特殊寄存器 */
    for (i = 0; i < 12; i++) {
        if (ADS1292_REG[i] != reg_cache[i]) {
            if (i != 0 && i != 8 && i != 11)
                res = 1;
            else
                continue;
        }
#ifdef DEBUG_ADS1292
        printf("%d %x\r\n", i, reg_cache[i]);
#endif
    }

#ifdef DEBUG_ADS1292
    if (res == 0)
        printf("REG write success\r\n");
    else
        printf("REG write err\r\n");
#endif

    return res;
}

/* ================================ ADS1292R 上电初始化区 ================================ */

/**
 * @brief  ADS1292R 上电初始化
 * @retval 无
 * @note   流程：STOP→RESET(硬件)→SDATAC→RESET(软件)→SDATAC→读默认寄存器
 */
void ADS1292_PowerOnInit(void)
{
#ifdef DEBUG_ADS1292
    uint8_t i;
    uint8_t reg_cache[12];
#endif

    ADS_START_LOW();    /* 停止数据转换 */
    ADS_RESET_LOW();    /* 硬件复位 */
    vTaskDelay(pdMS_TO_TICKS(1000));
#ifdef DEBUG_ADS1292
    printf("[DIAG] PWR: reset low done\r\n");
#endif
    ADS_RESET_HIGH();   /* 芯片上电，开始使能 */
    vTaskDelay(pdMS_TO_TICKS(100)); /* 等待稳定 */
#ifdef DEBUG_ADS1292
    printf("[DIAG] PWR: reset high done\r\n");
#endif

    ADS1292_Send_CMD(SDATAC);      /* 停止连续数据读取模式 */
    vTaskDelay(pdMS_TO_TICKS(10));
    ADS1292_Send_CMD(ADS_RESET);   /* 软件复位 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    ADS1292_Send_CMD(SDATAC);      /* 再次停止连续读取 */
    vTaskDelay(pdMS_TO_TICKS(100));

#ifdef DEBUG_ADS1292
    ADS1292_WR_REGS(RREG | REG_ID, 12, reg_cache);
    printf("read default REG:\r\n");
    for (i = 0; i < 12; i++)
        printf("%d %x\r\n", i, reg_cache[i]);
#endif
}

/* ================================ ADS1292R 采集模式函数区 ================================ */

/**
 * @brief  通道1内部1mV方波测试信号
 * @retval 0=成功, 1=寄存器写入失败
 * @note   ADS1292R 此函数仅通道2有效，通道1受呼吸电路影响
 */
uint8_t ADS1292_Single_Test(void)
{
    uint8_t res = 0;

    g_ads_cfg2.Int_Test = INT_TEST_ON;      /* 使能内部测试信号 */
    g_ads_ch1.MUX       = MUX_Test_signal;  /* 测试信号输入 */
    g_ads_ch2.MUX       = MUX_Test_signal;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;
    vTaskDelay(pdMS_TO_TICKS(10));

    return res;
}

/**
 * @brief  内部噪声测试（输入短路）
 * @retval 0=成功, 1=寄存器写入失败
 */
uint8_t ADS1292_Noise_Test(void)
{
    uint8_t res = 0;

    g_ads_cfg2.Int_Test = INT_TEST_OFF;     /* 关闭内部测试信号 */
    g_ads_ch1.MUX       = MUX_input_shorted; /* 输入短路 */
    g_ads_ch2.MUX       = MUX_input_shorted;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;
    vTaskDelay(pdMS_TO_TICKS(10));

    return res;
}

/**
 * @brief  正常信号采集模式
 * @retval 0=成功, 1=寄存器写入失败
 */
uint8_t ADS1292_Single_Read(void)
{
    uint8_t res = 0;

    g_ads_cfg2.Int_Test = INT_TEST_OFF;         /* 关闭内部测试信号 */
    g_ads_ch1.MUX       = MUX_Normal_input;     /* 正常电极输入 */
    g_ads_ch2.MUX       = MUX_Normal_input;

    if (ADS1292_WRITE_REGBUFF())
        res = 1;
    vTaskDelay(pdMS_TO_TICKS(10));

    return res;
}

/**
 * @brief  设置 ADS1292R 采集模式并启动转换
 * @param  mode 0=正常采集, 1=内部测试信号, 2=噪声测试
 * @retval 0=成功, 1=失败
 */
uint8_t Set_ADS1292_Collect(uint8_t mode)
{
    uint8_t res;

    vTaskDelay(pdMS_TO_TICKS(10));

    switch (mode) {
    case 0:
        res = ADS1292_Single_Read();
        break;
    case 1:
        res = ADS1292_Single_Test();
        break;
    case 2:
        res = ADS1292_Noise_Test();
        break;
    default:
        res = 1;
        break;
    }

    if (res)
        return 1; /* 寄存器写入失败 */

    ADS1292_Send_CMD(RDATAC); /* 进入连续读取模式 */
    vTaskDelay(pdMS_TO_TICKS(10));
    ADS1292_Send_CMD(START_CMD); /* 发送开始转换命令（硬件START引脚也需拉高） */
    vTaskDelay(pdMS_TO_TICKS(10));

    return 0;
}

/* ================================ ECG 任务实现区 ================================ */

/**
 * @brief  将无符号24位原始数据转换为有符号32位微伏值
 * @param  num 24位无符号原始数据（高8位为状态，实际只用低24位）
 * @retval 有符号32位值（符号扩展后的24位数据）
 */
int32_t get_volt(uint32_t num)
{
    int32_t temp = (int32_t)num;
    temp <<= 8;
    temp >>= 8;
    return temp;
}


/**
 * @brief  ECG 采集任务（FreeRTOS 任务函数）
 * @param  argument 未使用
 * @retval 无
 * @note   初始化 ADS1292R → 配置正常采集 → 使能 DRDY 中断 → 循环读取并发送
 */
