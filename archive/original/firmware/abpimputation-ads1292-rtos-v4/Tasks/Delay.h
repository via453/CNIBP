/*****************************************************************
  Copyright (c) [2026], [EUIN_模电课设] All rights reserved
  File Name   : Delay.h
  Description : 延时函数声明，提供微秒、毫秒、秒级延时接口
  History     :
      euin   2026-03-14   create
      euin   2026-03-14   优化代码结构，添加规范化注释
*****************************************************************/

#ifndef __DELAY_H
#define __DELAY_H

/* ================================ 函数声明区 ================================ */

/**
 * @brief  微秒级延时函数
 * @param  us 延时时长，单位：微秒，范围：0~233015
 * @retval 无
 */
void Delay_us(uint32_t us);

/**
 * @brief  毫秒级延时函数
 * @param  ms 延时时长，单位：毫秒，范围：0~4294967295
 * @retval 无
 */
void Delay_ms(uint32_t ms);

/**
 * @brief  秒级延时函数
 * @param  s 延时时长，单位：秒，范围：0~4294967295
 * @retval 无
 */
void Delay_s(uint32_t s);

#endif /* __DELAY_H */
