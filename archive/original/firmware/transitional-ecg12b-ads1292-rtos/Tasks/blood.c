/**
 * @file   blood.c
 * @brief  血氧采集与波形显示模块（移植自 stm32f103c8t6_MAX30102_PRO）
 * @note   已删除 TJC 串口屏代码和 OLED 显示代码，波形通过 printf 输出
 */

#include "blood.h"
#include "usart.h"

// 波形显示参数
#define WAVEFORM_GAIN          2      // 波形幅度放大倍数（可调：1~16）
#define WAVEFORM_SEND_INTERVAL 3      // 发送间隔（每N个采样点发送一次，5=50ms@100Hz）
#define WAVEFORM_DC_ALPHA      0.01f  // DC基线跟踪系数（已弃用，保留兼容）
#define WAVEFORM_BASELINE_WIN  50     // 基线滑动窗口大小（50点=500ms@100Hz）

uint16_t g_fft_index = 0;         	 	//fft数据计数

struct compx s1[FFT_N+16];           	//FFT输入数据数组，S[1]开始存，数据大小约
struct compx s2[FFT_N+16];           	//FFT输入数据数组，S[1]开始存，数据大小约

struct
{
	float 	Hp	;			//血红蛋白
	float 	HpO2;			//氧合血红蛋白

}g_BloodWave;//血液数据缓存

BloodData g_blooddata = {0};					//血液数据存储

#define CORRECTED_VALUE			47   			//校准血液参数

/*funcation start ------------------------------------------------------------*/

//I2C通信测试
void I2C_Communication_Test(void)
{
    uint8_t device_id = MAX30102_I2C_ReadByte(0xAE, 0xFF);
    // if(device_id == 0x15)
    // {
    //     printf("I2C Communication OK! MAX30102 Device ID: 0x%02X\r\n", device_id);
    // }
    // else
    // {
    //     printf("I2C Communication Failed! Device ID: 0x%02X (Expected: 0x15)\r\n", device_id);
    // }
}

//血液数据信息处理 - 实时采集数据
void blood_data_update(void)
{
	uint32_t timeout;

	// 采样计数器（用于50ms间隔发送）
	static uint8_t send_counter = 0;
	// 基线滑动窗口
	static float baseline_buf[WAVEFORM_BASELINE_WIN] = {0};
	static uint8_t baseline_wr = 0;
	static uint16_t baseline_cnt = 0;
	static float baseline_sum = 0;

	// 等待中断引脚变低（数据就绪），带超时保护
	timeout = 100000;
	while(MAX30102_INT_Read() == 1 && timeout > 0)
	{
		timeout--;
		vTaskDelay(1);
	}

	if(timeout == 0)
	{
		// 超时，直接读取
		max30102_read_fifo();
		if(fifo_red == 0 && fifo_ir == 0)
		{
			return;
		}
	}
	else
	{
		// 中断触发，读取FIFO数据
		max30102_read_fifo();
	}

	// 清除中断
	MAX30102_I2C_ReadByte(MAX30102_Device_address, REG_INTR_STATUS_1);

	// 简单的移动平均滤波（3点平均）
	static float red_buffer[3] = {0};
	static float ir_buffer[3] = {0};
	static uint8_t buf_index = 0;

	red_buffer[buf_index] = (float)fifo_red;
	ir_buffer[buf_index] = (float)fifo_ir;
	buf_index = (buf_index + 1) % 3;

	float filtered_red = (red_buffer[0] + red_buffer[1] + red_buffer[2]) / 3.0f;
	float filtered_ir = (ir_buffer[0] + ir_buffer[1] + ir_buffer[2]) / 3.0f;

	// DC基线跟踪（滑动窗口均值，窗口500ms@100Hz）
	// 先减去被覆盖的旧值
	baseline_sum -= baseline_buf[baseline_wr];
	// 写入新值
	baseline_buf[baseline_wr] = filtered_red;
	baseline_sum += filtered_red;
	// 移动写指针
	baseline_wr = (baseline_wr + 1) % WAVEFORM_BASELINE_WIN;
	// 计数（窗口填满前用实际计数求均值）
	if(baseline_cnt < WAVEFORM_BASELINE_WIN) baseline_cnt++;
	float dc_baseline = baseline_sum / baseline_cnt;

	// 每 WAVEFORM_SEND_INTERVAL 个采样点发送一次（50ms@100Hz）
	send_counter++;
	if(send_counter >= WAVEFORM_SEND_INTERVAL)
	{
		send_counter = 0;

		// 提取AC分量，放大，居中到128
		float ac = (filtered_red - dc_baseline) * WAVEFORM_GAIN;
		int waveform_val = 128 + (int)ac;

		// 限幅到 0~255
		if(waveform_val > 255) waveform_val = 255;
		if(waveform_val < 0)   waveform_val = 0;

#ifdef OUTPUT_TJC
		TJC_SendWaveform(1, (uint8_t)waveform_val);  /* s1 = PPG 波形 */
#else
		printf("%d\r\n", waveform_val);
#endif
	}
}

//血液信息转换（FFT-based SpO2计算，保留但不在 blood_Loop 中调用）
void blood_data_translate(void)
{
	float n_denom;
	uint16_t i;

	// 直流滤波
	float dc_red =0;
	float dc_ir =0;
	float ac_red =0;
	float ac_ir =0;

	for (i=0 ; i<FFT_N ; i++ )
	{
		dc_red += s1[i].real ;
		dc_ir +=  s2[i].real ;
	}
		dc_red =dc_red/FFT_N ;
		dc_ir =dc_ir/FFT_N ;

	for (i=0 ; i<FFT_N ; i++ )
	{
		s1[i].real =  s1[i].real - dc_red ;
		s2[i].real =  s2[i].real - dc_ir ;
	}

	//移动平均滤波
	for(i = 1;i < FFT_N-1;i++)
	{
			n_denom= ( s1[i-1].real + 2*s1[i].real + s1[i+1].real);
			s1[i].real=  n_denom/4.00;

			n_denom= ( s2[i-1].real + 2*s2[i].real + s2[i+1].real);
			s2[i].real=  n_denom/4.00;
	}
	//八点平均滤波
	for(i = 0;i < FFT_N-8;i++)
	{
			n_denom= ( s1[i].real+s1[i+1].real+ s1[i+2].real+ s1[i+3].real+ s1[i+4].real+ s1[i+5].real+ s1[i+6].real+ s1[i+7].real);
			s1[i].real=  n_denom/8.00;

			n_denom= ( s2[i].real+s2[i+1].real+ s2[i+2].real+ s2[i+3].real+ s2[i+4].real+ s2[i+5].real+ s2[i+6].real+ s2[i+7].real);
			s2[i].real=  n_denom/8.00;
	}

	// 打印滤波后数据（RED和IR各256点，用于调试显示）
	for(i = 0; i < FFT_N-8; i++)
	{
		printf("%.0f,%.0f\r\n", s1[i].real, s2[i].real);
	}
	//初始化变换显示
	g_fft_index = 0;
	//快速傅里叶变换
	FFT(s1);
	FFT(s2);

	//求功率谱 - 取FFT结果的模
	for(i = 0;i < FFT_N;i++)
	{
		s1[i].real=sqrtf(s1[i].real*s1[i].real+s1[i].imag*s1[i].imag);
		s2[i].real=sqrtf(s2[i].real*s2[i].real+s2[i].imag*s2[i].imag);
	}
	//计算交流分量
	for (i=1 ; i<FFT_N ; i++ )
	{
		ac_red += s1[i].real ;
		ac_ir +=  s2[i].real ;
	}

	// 检查是否有有效信号
	if(ac_red <= 0 || ac_ir <= 0 || dc_red <= 0 || dc_ir <= 0)
	{
		g_blooddata.heart = 0;
		g_blooddata.SpO2 = 0;
		return;
	}

	//获取峰值点的横坐标  纵坐标暂时不为
	int s1_max_index = find_max_num_index(s1, 30);
	int s2_max_index = find_max_num_index(s2, 30);

	//计算心率
	float Heart_Rate = 60.00 * ((100.0 * s1_max_index )/ FFT_N);

	g_blooddata.heart = Heart_Rate;

	//计算血氧饱和度
	// 精确R值公式: R = (ACred/DCred) / (ACir/DCir) = (ACred * DCir) / (ACir * DCred)
	float R = (ac_red * dc_ir) / (ac_ir * dc_red);

	// 根据R值计算SpO2 (经验公式)
	float sp02_num;
	if(R >= 0.4 && R <= 1.0)
	{
		// 标准范围公式
		sp02_num = -45.060 * R * R + 30.354 * R + 94.845;
	}
	else if(R < 0.4)
	{
		// R值偏小，SpO2接近100%
		sp02_num = 110.0 - 25.0 * R;
	}
	else
	{
		// R值偏大，信号可能不准
		sp02_num = 70.0;
	}

	// 限制范围到70-100%
	if(sp02_num > 100.0) sp02_num = 100.0;
	if(sp02_num < 70.0) sp02_num = 70.0;

	g_blooddata.SpO2 = sp02_num;
}


void blood_Loop(void)
{
	// 执行实时数据采集处理
	blood_data_update();
}
