#include"hadder\\tm1637.h"
/********************Start函数*************************/
void I2CStart()
{
 DIO=1;
 CLK=1;
 nop;
 DIO=1;
 nop;
 DIO=0;
 nop;
 CLK=0;
}

/********************Stop函数*************************/
void I2CStop()
{
    CLK=0;
        nop;
        nop;
        DIO=0;
        nop;
        nop;
        CLK=1;
        nop;
        nop;
        nop;
        DIO=1;
        nop;
        CLK=0;
        DIO=0;
}

/***************发送8bit数据，从低位开始**************/
void I2CWritebyte(unsigned char oneByte)
{
  unsigned char i,j;
  for(i=0;i<8;i++)
  {
    CLK=0;
        if(oneByte&0x01) 
          DIO=1;
        else 
          DIO=0;
        nop;
    CLK=1;
    oneByte=oneByte>>1;
  }
                                                                                  //8位数据传送完                 
        CLK = 0;                                                                //判断芯片发过来的ACK应答信号
        nop;
        while((DIO==1)&&j<100)
		{
		    j++;
		}
        nop;
        CLK = 1;
        nop;
}

/***************读按键程序**************/
uchar read_key()
{
	uchar rekey,i,j;
	I2CStart();
	I2CWritebyte(0x42);                                                         //写读键指令0x42
	DIO=1;
	for(i=0;i<8;i++)
	{
	        CLK=0;
	        nop;
	        nop;
	        rekey=rekey>>1;                                                           //先读低位
	        nop;
	        nop;
	        CLK=1;
	        if(DIO) 
	          rekey=rekey|0x80;
	        else 
	          rekey=rekey|0x00;
	        nop;
           }
	        CLK = 0;                                                          //判断芯片发过来的ACK应答信号
	        nop;
	        nop;
	        while((DIO==1)&&j<100)
			{
			    j++;
			}
	        nop;
	        nop;
	        CLK = 1;
	        nop;
	        nop;
	        I2CStop();
	        
	        return rekey;
}


/************显示函数，固定地址写数据************/
void TM1637_Set(uchar add, uchar value)
{
 I2CStart();
 I2CWritebyte(0x44);                                 //数据命令设置：固定地址，写数据到显示寄存器
 I2CStop();

 I2CStart();
 I2CWritebyte(add);                                //地址命令设置：写入add对应地址

 I2CWritebyte(value);                        //给add地址写数据
 I2CStop();

 I2CStart();
 I2CWritebyte(0x8C);                                //显示控制命令：开显示，脉冲宽度为11/16.
 I2CStop();
}