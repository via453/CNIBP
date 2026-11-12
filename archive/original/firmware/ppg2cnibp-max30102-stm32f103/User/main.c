#include "stm32f10x.h"
#include "exti.h"
#include "systick.h"
#include "usart.h"
#include "iic.h"
#include "Font.h"
#include "OLED_IIC.h"
#include "18b20.h"
#include "MAX30102.h"
#include "algorithm.h"
#include "blood.h"
#include "tjc.h"

int main(void)
{	
	int i=0;
	uint32_t timeout;
	
	USART_CONFIG();
	printf("\r\n========== MAX30102 ��ʼ����ʼ ==========\r\n");
	
#if TJC_MODE
	/* TJC串口屏初始化 - 屏幕上电自动初始化，USART已配置即可通信 */
	printf("TJC 串口屏已就绪\r\n");
#endif
	
	IIC_GPIO_INIT();
	printf("IIC GPIO ��ʼ�����\r\n");
	
	OLED_Init();	
	fill_picture(0x00);
	printf("OLED ��ʼ�����\r\n");

	Ds18b20_Init();
	DS18B20_Read_Temperature();
	
	// MAX30102 ��ʼ��
	MAX30102_GPIO();
	printf("MAX30102 GPIO ��ʼ�����\r\n");
	
	// ���MAX30102�Ƿ����
	if(MAX30102_Check() != 0)
	{
		printf("MAX30102 ���ʧ�ܣ�����Ӳ�����ӣ�\r\n");
		while(1); // ֹͣ����
	}
	
	// IICͨ�Ų���
	if(MAX30102_IIC_Test() != 0)
	{
		printf("MAX30102 IICͨ�Ų���ʧ�ܣ�\r\n");
	}
	
	// ��λMAX30102
	printf("���ڸ�λMAX30102...\r\n");
	if(!Max30102_reset())
	{
		printf("MAX30102 ��λʧ�ܣ�\r\n");
		while(1);
	}
	printf("MAX30102 ��λ���\r\n");
	
	// ����MAX30102
	printf("��������MAX30102...\r\n");
	MAX30102_Config();
	printf("MAX30102 �������\r\n");
	
	// ��֤����
	uint8_t mode = IIC_Read_Byte(MAX30102_Device_address, REG_MODE_CONFIG);
	uint8_t led1 = IIC_Read_Byte(MAX30102_Device_address, REG_LED1_PA);
	uint8_t led2 = IIC_Read_Byte(MAX30102_Device_address, REG_LED2_PA);
	uint8_t fifo_config = IIC_Read_Byte(MAX30102_Device_address, REG_FIFO_CONFIG);
	uint8_t int_en = IIC_Read_Byte(MAX30102_Device_address, REG_INTR_ENABLE_1);
	
	// printf("������֤:\r\n");
	// printf("  MODE_CONFIG: 0x%02X (0x03=SpO2ģʽ)\r\n", mode);
	// printf("  LED1_PA: 0x%02X (Red LED����)\r\n", led1);
	// printf("  LED2_PA: 0x%02X (IR LED����)\r\n", led2);
	// printf("  FIFO_CONFIG: 0x%02X\r\n", fifo_config);
	// printf("  INT_ENABLE: 0x%02X (0x40=PPG_RDY�ж�ʹ��)\r\n", int_en);
	// printf("�ж����ŵ�ǰ״̬: %d (��ָ���Ϻ�Ӧ��0)\r\n", MAX30102_INTPin_Read());
	// printf("��ʾ: ������ָ��ס�����������Ӧ�ó���\r\n");
	
	// �ȴ��û�������ָ����3��ʱ�䣩
	// printf("�ȴ�3�룬������ָ��ס������...\r\n");
	// for(i = 3; i > 0; i--)
	// {
	// 	printf("%d... ", i);
	// 	Delay_ms(1000);
	// }
	// printf("\r\n");
	
	// �ٴμ���ж�����״̬
	printf("��ָ���ú��ж�����״̬: %d\r\n", MAX30102_INTPin_Read());
	
	// Ԥ�� - ��ȡ32������������ʱ����
	//printf("��ʼԤ�ȣ���ȡ32������...\r\n");
	// for(i = 0; i < 32; i++) 
	// {
	// 	timeout = 100000; // ��ʱ����
	// 	while(MAX30102_INTPin_Read() == 1 && timeout > 0) // �ȴ��ж����ű��
	// 	{
	// 		timeout--;
	// 	}
		
	// 	if(timeout == 0)
	// 	{
	// 		printf("Ԥ�ȳ�ʱ����%d��������ȡʧ��\r\n", i);
	// 		printf("���飺1.���������� 2.��ָ�Ƿ���� 3.LED�Ƿ���������\r\n");
	// 		break;
	// 	}
		
	// 	// ��ȡFIFO����
	// 	max30102_read_fifo();
		
	// 	// ����жϣ���ȡ״̬�Ĵ�����
	// 	IIC_Read_Byte(MAX30102_Device_address, REG_INTR_STATUS_1);
	// }
	// printf("Ԥ����ɣ��ɹ���ȡ %d ������\r\n", i);
	// printf("========== ��ʼ����ɣ�������ѭ�� ==========\r\n\r\n");
	
	while(1)
	{	
		//deal_Temp();
		blood_Loop();
	}
}
