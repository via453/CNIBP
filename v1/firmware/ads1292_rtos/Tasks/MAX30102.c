#include "MAX30102.h"
#include "FreeRTOS.h"
#include "task.h"

uint16_t fifo_red;
uint16_t fifo_ir;

void MAX30102_GPIO(void)
{
	RCC_APB2PeriphClockCmd(MAX30102_INT_GPIO_CLK, ENABLE);
	GPIO_InitTypeDef GPIO_InitStruct;
	// 改为上拉输入，确保中断引脚有确定状态
	GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Pin   = MAX30102_INT_PIN;
	GPIO_Init(MAX30102_INT_GPIO_PORT, &GPIO_InitStruct);
}

uint8_t Max30102_reset(void)
{
	uint8_t retry = 0;

	// 写复位命令
	if (!MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_MODE_CONFIG, 0x40))
		return 0; // 写失败

	// 等待复位完成（bit 6 变为0）
	while (retry < 100)
	{
		uint8_t mode = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_MODE_CONFIG);
		if ((mode & 0x40) == 0) // 复位完成
			return 1;
		vTaskDelay(pdMS_TO_TICKS(10));
		retry++;
	}

	printf("MAX30102复位超时！\r\n");
	return 0;
}

/**
 * @brief  检测MAX30102模块是否存在
 * @retval 0: 检测到模块  1: 未检测到模块
 */
uint8_t MAX30102_Check(void)
{
	uint8_t part_id;
	uint8_t rev_id;

	// 读取 PART_ID 寄存器 (0xFF)，正常应返回 0x15
	part_id = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_PART_ID);
	// 读取 REV_ID 寄存器 (0xFE)
	rev_id = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_REV_ID);

	printf("MAX30102 PART_ID: 0x%02X (expected: 0x15)\r\n", part_id);
	printf("MAX30102 REV_ID: 0x%02X\r\n", rev_id);

	if (part_id == 0x15)
	{
		printf("MAX30102 detected OK!\r\n");
		return 0; // 成功
	}
	else
	{
		printf("MAX30102 NOT detected! Check your IIC connection.\r\n");
		return 1; // 失败
	}
}

/**
 * @brief  IIC通信测试 - 写寄存器再读回验证
 * @retval 0: 测试通过  1: 测试失败
 */
uint8_t MAX30102_IIC_Test(void)
{
	uint8_t temp;
	uint8_t test_val = 0x05;
	uint8_t read_back;

	// 先保存原值
	temp = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_FIFO_WR_PTR);

	// 写入测试值
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_FIFO_WR_PTR, test_val);

	// 读回验证
	read_back = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_FIFO_WR_PTR);

	// 恢复原值
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_FIFO_WR_PTR, temp);

	printf("IIC Test: Write 0x%02X, Read 0x%02X ", test_val, read_back);

	if (read_back == test_val)
	{
		printf("- PASS\r\n");
		return 0; // 成功
	}
	else
	{
		printf("- FAIL\r\n");
		return 1; // 失败
	}
}

void MAX30102_Config(void)
{
	uint8_t temp;

	// 1. 先关中断
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_INTR_ENABLE_1, 0x00);
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_INTR_ENABLE_2, 0x00);

	// 2. 清除中断状态（读状态寄存器）
	temp = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_INTR_STATUS_1);
	temp = MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_INTR_STATUS_2);

	// 3. 复位FIFO指针
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_FIFO_WR_PTR, 0x00);
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_OVF_COUNTER, 0x00);
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_FIFO_RD_PTR, 0x00);

	// 4. 配置FIFO: sample avg = 1, fifo rollover = false, fifo almost full = 17
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_FIFO_CONFIG, 0x0f);

	// 5. 先配置为HR模式（Red only），测试红灯是否正常
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_MODE_CONFIG, 0x02); // HR模式

	// 6. 配置SpO2参数 - 减小ADC范围避免溢出
	// 0x2A = 00101010: ADC range = 2048nA (更小范围，避免饱和), sample rate = 100Hz, pulse width = 200uS
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_SPO2_CONFIG, 0x2A);

	// 7. 配置LED电流（降低强度避免溢出）
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_LED1_PA, 0x2F); // Red LED ~4.7mA
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_LED2_PA, 0x2F); // IR LED ~4.7mA

	// 8. 使能PPG_RDY中断
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_INTR_ENABLE_1, 0x40);
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_INTR_ENABLE_2, 0x00);

	// 9. 等待一下让LED稳定
	vTaskDelay(pdMS_TO_TICKS(50));

	// 10. 最后切换到SpO2模式（Red + IR）
	MAX30102_I2C_WriteByte(MAX30102_Device_address, REG_MODE_CONFIG, 0x03);

	// 11. 再等待一下
	vTaskDelay(pdMS_TO_TICKS(50));
}

void max30102_read_fifo(void)
{
	uint32_t un_temp; // 改为32位，避免溢出
	fifo_red = 0;
	fifo_ir = 0;
	uint8_t ach_i2c_data[6];

	// 直接读取FIFO数据寄存器
	MAX30102_I2C_ReadArray(MAX30102_Device_address, REG_FIFO_DATA, ach_i2c_data, 6);

	// 解析RED数据 (18位): [MSB][LSB][X] -> 取前18位
	un_temp = ((uint32_t)ach_i2c_data[0] << 16) | ((uint32_t)ach_i2c_data[1] << 8) | ach_i2c_data[2];
	fifo_red = (uint16_t)(un_temp >> 2); // 右移2位，得到18位数据的高16位

	// 解析IR数据 (18位): [MSB][LSB][X] -> 取前18位
	un_temp = ((uint32_t)ach_i2c_data[3] << 16) | ((uint32_t)ach_i2c_data[4] << 8) | ach_i2c_data[5];
	fifo_ir = (uint16_t)(un_temp >> 2); // 右移2位，得到18位数据的高16位
}
