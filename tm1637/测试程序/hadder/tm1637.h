#ifndef __TM1637_H__
#define __TM1637_H__
#include"STC15W.h"
#include<intrins.h>
#define uchar unsigned char
#define uint  unsigned int
sbit CLK = P3^0;
sbit DIO = P3^1;
#define nop _nop_();_nop_();_nop_();_nop_();_nop_();                 //宏定义
//uchar code TMaddr[6]={0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,};//六位数显地址
//uchar code TMkey[4]={0xf3,0xf4,0xf5,0xf0};	  ///四个键值 

void I2CStart();
void I2CStop();
void I2CWritebyte(unsigned char oneByte);
uchar read_key();
void TM1637_Set(uchar add, uchar value);

#endif