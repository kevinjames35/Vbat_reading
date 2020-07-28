
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <unistd.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdbool.h>

#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <search.h>


#define int8_t  		char
#define int16_t 		short
#define int32_t 		int
#define int64_t  		long long
#define uint8_t  		unsigned char
#define uint16_t 		unsigned short
#define uint32_t 		unsigned int
#define uint64_t 		unsigned long long


#define SIO_NCA6776
/***********************************************************************/
/***********  Define SuperIO Logical Device  ***************************/
/***********************************************************************/
/***** NCT-6776 LDN define ******/
#if defined(SIO_NCA6776)
	#define	SIO_LDN_GPIO6789	0x07	
	#define SIO_LDN_WDT		0x08
	#define SIO_LDN_GPIO2345	0x09
	#define	SIO_LDN_ACPI		0x0A	
	#define SIO_LDN_HWM		0x0B
	#define SIO_LDN_GPIO_PP		0x0F	//select GPIO Push-Pull or Open-Drain	
	#define SIO_LDN_GPIOA		0x17
#endif

/**********************************/
/**** NCT-6776 Raw Access file ****/
/**********************************/
#define REG_VCORE	0x20
#define REG_VIN0	0x21	//8mv,1step
#define REG_VCC3	0x23	//16mv,1step
#define REG_VIN1	0x24	//8mv,1step
#define REG_VIN2	0x25	//8mv,1step
#define REG_VIN3	0x26	//8mv,1step
#define REG_VSB3V	0xFF	//16mv,1step, bank5, 0x50
#define REG_VBAT	0xFF	//16mv,1step, bank5, 0x51

//Register address define
//Logical Device 8(gpio0,gpio1)
#define REG_GPIO0_WRITE		0xe1
#define REG_GPIO0_READ		0xe1
#define REG_GPIO1_WRITE		0xf1
#define REG_GPIO1_READ		0xf1
//Logical Device 9(gpio2,gpio3,gpio4,gpio5)
#define REG_GPIO2_WRITE		0xe1
#define REG_GPIO2_READ		0xe1
#define REG_GPIO3_WRITE		0xe5
#define REG_GPIO3_READ		0xe5
#define REG_GPIO4_WRITE		0xf1
#define REG_GPIO4_READ		0xf1
#define REG_GPIO5_WRITE		0xf5
#define REG_GPIO5_READ		0xf5
//Logical Device 7(gpio6,gpio7,gpio8,gpio9)
#define REG_GPIO6_WRITE		0xf5
#define REG_GPIO6_READ		0xf5
#define REG_GPIO7_WRITE		0xe1
#define REG_GPIO7_READ		0xe1
#define REG_GPIO8_WRITE		0xe5
#define REG_GPIO8_READ		0xe5
#define REG_GPIO9_WRITE		0xe9
#define REG_GPIO9_READ		0xe9
//Logical Device 17(gpioa)
#define REG_GPIOA_WRITE		0xe1
#define REG_GPIOA_READ		0xe1

/*** Vin R1/R2 Vaule setting ***/
#define	Vin0_R1		110	
#define	Vin0_R2		10
#define	Vin1_R1		402	
#define	Vin1_R2		100
#define	Vin2_R1		0	
#define	Vin2_R2		10
#define	Vin3_R1		200	
#define	Vin3_R2		100
#define	Vin4_R1		10	
#define	Vin4_R2		10
#define	Vin5_R1		10	
#define	Vin5_R2		10
#define	VinDEF_R1	10	
#define	VinDEF_R2	10

uint16_t SIO_INDEX 	=	0x2e ;
uint16_t SIO_DATA 	= 	0x2f;

void __sio_unlock(void);
void __sio_lock(void);
void __sio_logic_device(char num);
void _initial_SIObase(uint16_t uwSioBase);
void __sio_set_gpio(int PinOffset, int OutValue);
/********************************************************************/
/***** SuperIO access common functions ******************************/
/********************************************************************/
void __sio_unlock(void)
{
	outb(SIO_INDEX, 0x87);
	outb(SIO_INDEX, 0x87);
}
/***********/
void __sio_lock(void)
{
	outb(SIO_INDEX, 0xaa);
}
/***********/
void __sio_logic_device(char num)
{
	outb(SIO_INDEX, 0x7);
	outb(SIO_DATA, num);
}
/************/
uint8_t read_sio_reg(uint8_t LDN, uint8_t reg)
{
        outb(SIO_INDEX, 0x7);
	outb(SIO_DATA, LDN);
        outb(SIO_INDEX, reg);
        return inb(SIO_DATA);
}
/************/
uint8_t write_sio_reg(uint8_t LDN, uint8_t reg, uint8_t value)
{	
       	outb(SIO_INDEX, 0x7);
        outb(SIO_DATA, LDN);
        outb(SIO_INDEX, reg);
        outb(SIO_DATA, value);
        return 0;
}



static uint16_t wHWM_PORT  = 0;
static uint16_t wHWM_INDEX = 0;
static uint16_t wHWM_DATA  = 0;
/**************************/
void __getting_hwm_index(void)
{
uint16_t wAddr, xData;
	__sio_unlock();
	__sio_logic_device(SIO_LDN_HWM);
	outb(SIO_INDEX, 0x60);
	xData=(uint16_t)inb(SIO_DATA);
	outb(SIO_INDEX, 0x61);
	wAddr=(uint16_t)inb(SIO_DATA);
	wAddr= wAddr | (xData << 8 ) ;
	__sio_lock();
	if ( wAddr == 0 ) return ;
	wHWM_PORT = wAddr ;
	wHWM_INDEX = wAddr + 5;
	wHWM_DATA = wAddr + 6;
}

/******************************************/
/***** SuperIO get Voltage value **********/
/******************************************/
bool __sio_get_voltage( int* pdwData)
{
uint8_t xch;

	if ( wHWM_PORT == 0 ) __getting_hwm_index();
	if ( wHWM_PORT == 0 ) return false;

		__sio_set_gpio(0x33,0);
		sleep(1);
		outb( wHWM_INDEX, 0x4e);
		outb( wHWM_DATA, 0x05);	//bank 5, index 51
		outb( wHWM_INDEX, 0x51); //Vbat
		xch=inb(wHWM_DATA);
		outb( wHWM_INDEX, 0x4e);
		outb( wHWM_DATA, 0x00); //bank 0
		*pdwData=(int32_t)(xch * 16) ;
		//__sio_set_gpio(0x33,0);

	return true;
}



/******************************************/
/***** SuperIO set GPIO  value ************/
/******************************************/
void __sio_set_gpio(int PinOffset, int OutValue)
{
uint8_t gpGroup, gpPin;
uint8_t xReg, bData;
	gpGroup = (uint8_t)(PinOffset / 0x10);
	gpPin   = (uint8_t)(PinOffset % 0x10);
	__sio_unlock();		
	switch (gpGroup) {
	case 2:	
		xReg = REG_GPIO2_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO2345);
		break;
	case 3:	
		xReg = REG_GPIO3_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO2345);
		break;
	case 4:	
		xReg = REG_GPIO4_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO2345);
		break;
	case 5:	
		xReg = REG_GPIO5_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO2345);
		break;
	case 6:	
		xReg = REG_GPIO6_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO6789);
		break;
	case 7:	
		xReg = REG_GPIO7_WRITE ;
		__sio_logic_device(SIO_LDN_GPIO6789);
		break;
	default:
		return; //don't do anything
		break;
	}
	outb( SIO_INDEX, xReg);
	bData = inb(SIO_DATA);
	if ( OutValue ) bData |= (0x01 << gpPin);
	else		bData &=  ~(0x01 << gpPin);		
	outb( SIO_INDEX, xReg);
	outb( SIO_DATA, bData);

	__sio_lock();
	return;
}


int main(int argc, char *argv[])
{
int xi;





	if ( argc < 2 ) {
		//__printf_usage(argv[0]);
		return -1;
	}
	
	for ( xi= 1; xi< argc ; xi++ ) {
		if( strcmp("-e", argv[xi]) == 0 ) 
		{}
			}
return 0;
}
