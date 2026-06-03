#ifndef __STC15F2K60S2EEPROM_H_
#define __STC15F2K60S2EEPROM_H_
#ifndef uchar
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif
//#define ENABLE_IAP 0x80           //if SYSCLK<30MHz
//#define ENABLE_IAP 0x81           //if SYSCLK<24MHz
//#define ENABLE_IAP  0x82            //if SYSCLK<20MHz
#define ENABLE_IAP 0x83           //if SYSCLK<12MHz
//#define ENABLE_IAP 0x84           //if SYSCLK<6MHz
//#define ENABLE_IAP 0x85           //if SYSCLK<3MHz
//#define ENABLE_IAP 0x86           //if SYSCLK<2MHz
//#define ENABLE_IAP 0x87           //if SYSCLK<1MHz
#define CMD_IDLE    0               //¿ÕÏÐÄ£Ê½
#define CMD_READ    1               //IAP×Ö½Ú¶ÁÃüÁî
#define CMD_PROGRAM 2               //IAP×Ö½Ú±à³ÌÃüÁî
#define CMD_ERASE   3               //IAPÉÈÇø²Á³ýÃüÁî
void IapIdle();
uchar IapReadByte(uint addr);
void IapProgramByte(uint addr, uchar dat);
void IapEraseSector(uint addr);
#define IAP_ADDRESS 0x0000
#endif