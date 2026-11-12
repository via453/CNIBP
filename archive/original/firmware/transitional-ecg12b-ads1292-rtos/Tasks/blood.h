#ifndef _BLOOD_H
#define _BLOOD_H

#include "stm32f10x.h"                  // Device header
#include "MAX30102.h"
#include "algorithm.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"

typedef enum
{
	BLD_NORMAL,		//正常
	BLD_ERROR,		//错误状态

}BloodState;//状态枚举

typedef struct
{
	int 		heart;		//心率
	float 			SpO2;			//血氧
}BloodData;


void blood_data_translate(void);
void blood_data_update(void);
void blood_Loop(void);
void I2C_Communication_Test(void);

#endif
