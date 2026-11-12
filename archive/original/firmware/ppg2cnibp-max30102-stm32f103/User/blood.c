#include "blood.h"
#include "usart.h"
#include "tjc.h"
uint16_t g_fft_index = 0;         	 	//fft��������±�

struct compx s1[FFT_N+16];           	//FFT������������S[1]��ʼ��ţ����ݴ�С�Լ�����
struct compx s2[FFT_N+16];           	//FFT������������S[1]��ʼ��ţ����ݴ�С�Լ�����

struct
{
	float 	Hp	;			//Ѫ�쵰��	
	float 	HpO2;			//����Ѫ�쵰��
	
}g_BloodWave;//ѪҺ��������

BloodData g_blooddata = {0};					//ѪҺ���ݴ洢

#define CORRECTED_VALUE			47   			//�궨ѪҺ��������

/*funcation start ------------------------------------------------------------*/
//I2Cͨ����֤
void I2C_Communication_Test(void)
{
    uint8_t device_id = IIC_Read_Byte(0xAE, 0xFF);
    // if(device_id == 0x15)
    // {
    //     printf("I2C Communication OK! MAX30102 Device ID: 0x%02X\r\n", device_id);
    // }
    // else
    // {
    //     printf("I2C Communication Failed! Device ID: 0x%02X (Expected: 0x15)\r\n", device_id);
    // }
}

//ѪҺ�����Ϣ���� - ����ʵʱ���
void blood_data_update(void)
{
	uint32_t timeout;
	static float last_red = 0;
	static float last_ir = 0;
	
	// ��ȡһ������
	// �ȴ��ж����ű�ͣ������ݾ�����������ʱ����
	timeout = 100000;
	while(MAX30102_INTPin_Read() == 1 && timeout > 0)
	{
		timeout--;
	}
	
	if(timeout == 0)
	{
		// ��ʱ������ֱ�Ӷ�ȡ
		max30102_read_fifo();
		if(fifo_red == 0 && fifo_ir == 0)
		{
			return;
		}
	}
	else
	{
		// �ж�������������ȡFIFO����
		max30102_read_fifo();
	}
	
	// ����ж�
	IIC_Read_Byte(MAX30102_Device_address, REG_INTR_STATUS_1);
	
	// �򵥵��ƶ�ƽ���˲���3��ƽ����
	static float red_buffer[3] = {0};
	static float ir_buffer[3] = {0};
	static uint8_t buf_index = 0;
	
	red_buffer[buf_index] = (float)fifo_red;
	ir_buffer[buf_index] = (float)fifo_ir;
	buf_index = (buf_index + 1) % 3;
	
	float filtered_red = (red_buffer[0] + red_buffer[1] + red_buffer[2]) / 3.0f;
	float filtered_ir = (ir_buffer[0] + ir_buffer[1] + ir_buffer[2]) / 3.0f;
	
	// ʵʱ����˲��������
	//printf("%.0f,%.0f\r\n", filtered_red, filtered_ir);
	printf("%.0f\r\n", filtered_red);
}

//ѪҺ��Ϣת��
void blood_data_translate(void)
{	
	float n_denom;
	uint16_t i;
	
	// printf("���� blood_data_translate()\r\n");
	// 4 pt Moving Average
//	printf("****************red******************************************************************\r\n");
//	for(i = 0;i < FFT_N;i++) 
//	{
//		printf("%f\r\n",s1[i].real);	
//	}
//	printf("***************** ir*****************************************************************\r\n");
//	for(i = 0;i < FFT_N;i++) 
//	{
//		printf("%f\r\n",s2[i].real);	
//	}
	//ֱ���˲�
	float dc_red =0; 
	float dc_ir =0;
	float ac_red =0; 
	float ac_ir =0;
	
	// ���ԣ���ӡֱ���˲�ǰ��ԭʼ����ǰ5���ͺ�5��
	// printf("ԭʼ����(ǰ5): ");
	// for(i = 0; i < 5; i++) printf("%.0f ", s1[i].real);
	// printf("... ");
	// for(i = FFT_N-5; i < FFT_N; i++) printf("%.0f ", s1[i].real);
	// printf("\r\n");
	
	for (i=0 ; i<FFT_N ; i++ ) 
	{
		dc_red += s1[i].real ;
		dc_ir +=  s2[i].real ;
	}
		dc_red =dc_red/FFT_N ;
		dc_ir =dc_ir/FFT_N ;
		
		// ���ԣ���ӡֱ������
		// printf("ֱ������: dc_red=%.0f, dc_ir=%.0f\r\n", dc_red, dc_ir);
		
	for (i=0 ; i<FFT_N ; i++ )  
	{
		s1[i].real =  s1[i].real - dc_red ; 
		s2[i].real =  s2[i].real - dc_ir ; 
	}
	
	//�ƶ�ƽ���˲�
	// printf("***********8 pt Moving Average red******************************************************\r\n");
	for(i = 1;i < FFT_N-1;i++) 
	{
			n_denom= ( s1[i-1].real + 2*s1[i].real + s1[i+1].real);
			s1[i].real=  n_denom/4.00; 
			
			n_denom= ( s2[i-1].real + 2*s2[i].real + s2[i+1].real);
			s2[i].real=  n_denom/4.00; 			
	}
	//�˵�ƽ���˲�
	for(i = 0;i < FFT_N-8;i++) 
	{
			n_denom= ( s1[i].real+s1[i+1].real+ s1[i+2].real+ s1[i+3].real+ s1[i+4].real+ s1[i+5].real+ s1[i+6].real+ s1[i+7].real);
			s1[i].real=  n_denom/8.00; 
			
			n_denom= ( s2[i].real+s2[i+1].real+ s2[i+2].real+ s2[i+3].real+ s2[i+4].real+ s2[i+5].real+ s2[i+6].real+ s2[i+7].real);
			s2[i].real=  n_denom/8.00; 
	}
	
	// ��ӡ�˲�������ݣ�RED��IR��256���㣬���ڲ�����ʾ��
	for(i = 0; i < FFT_N-8; i++) 
	{
		printf("%.0f,%.0f\r\n", s1[i].real, s2[i].real);
	}
	//��ʼ�任��ʾ	
	g_fft_index = 0;	
	//���ٸ���Ҷ�任
	FFT(s1);
	FFT(s2);
	
	//��ƽ�� - ����FFT�����ģ
	// printf("��ʼFFT�㷨****************************************************************************************************\r\n");
	for(i = 0;i < FFT_N;i++) 
	{
		s1[i].real=sqrtf(s1[i].real*s1[i].real+s1[i].imag*s1[i].imag);
		s2[i].real=sqrtf(s2[i].real*s2[i].real+s2[i].imag*s2[i].imag);  // �޸���ԭ����s1[i].real��Ӧ����s2[i].real
	}
	//���㽻������
	for (i=1 ; i<FFT_N ; i++ ) 
	{
		ac_red += s1[i].real ;
		ac_ir +=  s2[i].real ;
	}
	
	// ���ԣ���ӡ���������͹ؼ��������
	// printf("��������: ac_red=%.0f, ac_ir=%.0f\r\n", ac_red, ac_ir);
	
	// ����Ƿ�����Ч�ź�
	if(ac_red <= 0 || ac_ir <= 0 || dc_red <= 0 || dc_ir <= 0)
	{
		// printf("����: �ź�̫����������! ac_red=%.0f, ac_ir=%.0f, dc_red=%.0f, dc_ir=%.0f\r\n", 
		// 	   ac_red, ac_ir, dc_red, dc_ir);
		g_blooddata.heart = 0;
		g_blooddata.SpO2 = 0;
		return;
	}
	
	// for(i = 0;i < FFT_N/2;i++) 
	// {
	// 	printf("%f\r\n",s1[i].real);
	// }
	// printf("****************************************************************************************\r\n");
	// for(i = 0;i < FFT_N/2;i++) 
	// {
	// 	printf("%f\r\n",s2[i].real);	
	// }
	
	// printf("����FFT�㷨***************************************************************************************************************\r\n");
//	for(i = 0;i < 50;i++) 
//	{
//		if(s1[i].real<=10)
//			break;
//	}
	
	//printf("%d\r\n",(int)i);
	//��ȡ��ֵ��ĺ�����  �������������Ϊ 
	int s1_max_index = find_max_num_index(s1, 30);
	int s2_max_index = find_max_num_index(s2, 30);
	// printf("%d\r\n",s1_max_index);
	// printf("%d\r\n",s2_max_index);
	//���HbO2��Hb�ı仯Ƶ���Ƿ�һ��
//	if(i>=45)
//	{
		//���ʼ���
//		uint16_t Heart_Rate = 60.00 * SAMPLES_PER_SECOND * 
//													s1_max_index / FFT_N;
		
		float Heart_Rate = 60.00 * ((100.0 * s1_max_index )/ FFT_N);
		
		g_blooddata.heart = Heart_Rate;
		
		//Ѫ����������
		//double R = (s2[average].real * s1[0].real)/(s1[s1_max_index].real * s2[0].real);		
		//sp02_num = (1 - sp02_num) * SAMPLES_PER_SECOND + CORRECTED_VALUE;
		
		//double R = (s1[s1_max_index].real/s1[0].real)/(s2[s2_max_index].real/s2[0].real);
		
		//double sp02_num =-16.666666*R*R+ 8.333333 *R + 100;
		//g_blooddata.SpO2 = sp02_num;
		//Ѫ����������
	//float sp02_num = (s2[s1_max_index].real * s1[0].real)/(s1[s1_max_index].real * s2[0].real);
			
			//sp02_num = (1 - sp02_num) * SAMPLES_PER_SECOND + CORRECTED_VALUE;
			//( n_y_ac *n_x_dc_max) / ( n_x_ac *n_y_dc_max)
			
			// ��ȷRֵ��ʽ: R = (ACred/DCred) / (ACir/DCir) = (ACred * DCir) / (ACir * DCred)
			float R = (ac_red * dc_ir) / (ac_ir * dc_red);
			
			// ����Rֵ����SpO2 (���鹫ʽ)
			float sp02_num;
			if(R >= 0.4 && R <= 1.0)
			{
				// ��׼��Χ��ʽ
				sp02_num = -45.060 * R * R + 30.354 * R + 94.845;
			}
			else if(R < 0.4)
			{
				// RֵƫС��SpO2�ӽ�100%
				sp02_num = 110.0 - 25.0 * R;
			}
			else
			{
				// Rֵƫ���źſ��ܲ���
				sp02_num = 70.0;
			}
			
			// ���Ʒ�Χ��70-100%
			if(sp02_num > 100.0) sp02_num = 100.0;
			if(sp02_num < 70.0) sp02_num = 70.0;
			
			g_blooddata.SpO2 = sp02_num;
			
			// ���ԣ���ӡѪ���������
			// printf("Ѫ������: R=%.4f, SpO2=%.2f%%\r\n", R, sp02_num);
			
		//״̬����
//	}
//	else //���ݷ����쳣
//	{
//		g_blooddata.heart = 0;
//		g_blooddata.SpO2 	= 0;
//	}
	//�����任��ʾ
}


void blood_Loop(void)
{
	// ����ʵʱ�ɼ����������
	blood_data_update();
	
	// ����OLED��ʾ���������ָ���ã�
	if(g_blooddata.heart > 0)
	{
		OLED_Printf_EN(2,0,"heart:%3d/min  ",g_blooddata.heart);
		OLED_Printf_EN(4,0,"SpO2:%2.2f%%  ",g_blooddata.SpO2);
	}

	/* TJC串口屏显示 */
	#if TJC_MODE
	TJC_SetWaveform(0, TJC_NormalizePPG(fifo_red));
	if(g_blooddata.SpO2 > 0)
	{
		TJC_SetNumber("n0", (int)g_blooddata.SpO2);
	}
	#endif
}

