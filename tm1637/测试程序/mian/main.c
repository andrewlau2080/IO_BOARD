#include"hadder\\STC15W.h"
#include"hadder\\tm1637.h"

#define uchar unsigned char
#define uint unsigned int

uchar code CODE[19] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x77,0x7C,0x39,0x5E,0x79,0x71,0x76,0x38,0x73};	 //0--18

sbit OUT1= P3^7; //
sbit OUT2 = P3^6;  //
sbit IN1 = P5^5;
sbit beep=P5^4;
/************************************************************************* 
                  数据定义
*************************************************************************/
uchar time1,time2;

uchar update_dis;
uchar key_timeto;
uchar mode;

bit write=0;
bit flag;
 uchar i=1;

/********************************************************************
      定时器t0、t1都使用模式0十六位自动重装。初始化函数
*********************************************************************/
void Timer0Init(void)		//10ms
{
    AUXR &= 0x7F;		//定时器时钟12T模式
	TMOD &= 0xF0;		//设置定时器模式
	TL0 = 0xF0;		//设置定时初值
	TH0 = 0xD8;		//设置定时初值
	TF0 = 0;		//清除TF0标志
	TR0 = 1;		//定时器0开始计时
	ET0 = 1;
	EA = 1;
}

/********************************************************************
                           主函数
*********************************************************************/
void main()
{	
	
	uchar key,key_L;
	Timer0Init();
	beep=1;
	OUT1=1;
	OUT2=1;
  TM1637_Set(0xc0,CODE[0]);
  TM1637_Set(0xc1,CODE[0]);
  TM1637_Set(0xc2,CODE[0]);		
  TM1637_Set(0xc3,CODE[0]);
  TM1637_Set(0xc4,CODE[0]);		
  TM1637_Set(0xc5,CODE[0]);
	while(1)
	{ 	


	    if(key_timeto)
		{
			key_timeto = 0;
			key_L = key;
			 CLK=1;
		     DIO=1;
			key =read_key();
			if((key == 0xf3)&&(key_L == 0xf3))	  //此键值和数据手册上对应
			{	
				time2=1;	
				
			}
			//第二个按键按下
		   else if((key == 0xf4)&&(key_L != 0xf4))
			{	
				
			time2=2;
			}
			//第三个按键按下
		  else	 if((key == 0xf5)&&(key_L != 0xf5))
			{	
				
			    time2=3;
			}

		else	if((key == 0xf0)&&(key_L == 0xf0))	  //此键值和数据手册上对应
			{	
			      time2=4;	
				
			}
		}
		if(update_dis)
		{
			   update_dis = 0;

			    TM1637_Set(0xc0,CODE[0]);
				TM1637_Set(0xc1,CODE[0]);
			    TM1637_Set(0xc2,CODE[0]);		
				TM1637_Set(0xc3,CODE[0]);
				TM1637_Set(0xc4,CODE[time2/10]);		
				TM1637_Set(0xc5,CODE[time2%10]);	


		}
	}
}
/********************************************************************
                            定时器t0的中断处理
*********************************************************************/
void timer0() interrupt 1														   //中断
{																				
   static uint  cnt,cnt1;
   
   
  if(flag==0)
  {
    cnt1++;
    if(cnt1>=50)
    {
     cnt1=0;
	 if(i<9)
	 {
	  TM1637_Set(0xc0,CODE[i]);
	  TM1637_Set(0xc1,CODE[i]);
      TM1637_Set(0xc2,CODE[i]);		
	  TM1637_Set(0xc3,CODE[i]);
	  TM1637_Set(0xc4,CODE[i]);		
	  TM1637_Set(0xc5,CODE[i]);
	 }
	 else 
	 {
	  TM1637_Set(0xc0,0xff);
	  TM1637_Set(0xc1,0xff);
      TM1637_Set(0xc2,0xff);		
	  TM1637_Set(0xc3,0xff);
	  TM1637_Set(0xc4,0xff);		
	  TM1637_Set(0xc5,0xff);
	 }
	 i++;
	 if(i>10)
	 {
	  i=0;
	  flag=1;
	  }
     }
   }

 if(flag)
 {  
   cnt++;
   if(++cnt>=4)
   {
   	 cnt=0;
	 key_timeto = 1;
	 update_dis = 1;
   }
  }
}
