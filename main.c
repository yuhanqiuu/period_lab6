#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../Common/Include/stm32l051xx.h"

#define F_CPU 32000000L



void delay(int dly)
{
	while( dly--);
}

void wait_1ms(void)
{
	// For SysTick info check the STM32L0xxx Cortex-M0 programming manual page 85.
	SysTick->LOAD = (F_CPU/1000L) - 1;  // set reload register, counter rolls over from zero, hence -1
	SysTick->VAL = 0; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while((SysTick->CTRL & BIT16)==0); // Bit 16 is the COUNTFLAG.  True when counter rolls over from zero.
	SysTick->CTRL = 0x00; // Disable Systick counter
}

void waitms(int len)
{
	while(len--) wait_1ms();
}

#define PIN_PERIOD (GPIOA->IDR&BIT8)

// GetPeriod() seems to work fine for frequencies between 300Hz and 600kHz.
// 'n' is used to measure the time of 'n' periods; this increases accuracy.
long int GetPeriod (int n)
{
	int i;
	unsigned char overflow;
	unsigned int saved_TCNT1a, saved_TCNT1b;
	
	SysTick->LOAD = 0xffffff;  // 24-bit counter set to check for signal present
	overflow = 0;
	
	SysTick->VAL = 0xffffff; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD!=0) // Wait for square wave to be 0
	{
		if(SysTick->CTRL & BIT16) overflow++;
		if (overflow>10) return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	SysTick->LOAD = 0xffffff;  // 24-bit counter set to check for signal present
	overflow = 0;
	
	SysTick->VAL = 0xffffff; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD==0) // Wait for square wave to be 1
	{
		if(SysTick->CTRL & BIT16) overflow++;
		if (overflow>10) return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter
	
	SysTick->LOAD = 0xffffff;  // 24-bit counter reset
	overflow = 0;
	
	SysTick->VAL = 0xffffff; // load the SysTick counter to initial value
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	for(i=0; i<n; i++) // Measure the time of 'n' periods
	{
		while (PIN_PERIOD!=0) // Wait for square wave to be 0
		{
			if(SysTick->CTRL & BIT16) overflow++;
			if (overflow>10) return 0;
		}
		while (PIN_PERIOD==0) // Wait for square wave to be 1
		{
			if(SysTick->CTRL & BIT16) overflow++;
			if (overflow>10) return 0;
		}
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	return (overflow*0x01000000)+(0xffffff-SysTick->VAL);
}

void Configure_Pins (void)
{
	RCC->IOPENR |= BIT0; // peripheral clock enable for port A
	
	// Make pins PA0 to PA5 outputs (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0)
    GPIOA->MODER = (GPIOA->MODER & ~(BIT0|BIT1)) | BIT0; // PA0
	GPIOA->OTYPER &= ~BIT0; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT2|BIT3)) | BIT2; // PA1
	GPIOA->OTYPER &= ~BIT1; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT4|BIT5)) | BIT4; // PA2
	GPIOA->OTYPER &= ~BIT2; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT6|BIT7)) | BIT6; // PA3
	GPIOA->OTYPER &= ~BIT3; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT8|BIT9)) | BIT8; // PA4
	GPIOA->OTYPER &= ~BIT4; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT10|BIT11)) | BIT10; // PA5
	GPIOA->OTYPER &= ~BIT5; // Push-pull
}

// LQFP32 pinout
//             ----------
//       VDD -|1       32|- VSS
//      PC14 -|2       31|- BOOT0
//      PC15 -|3       30|- PB7
//      NRST -|4       29|- PB6
//      VDDA -|5       28|- PB5
//       PA0 -|6       27|- PB4
//       PA1 -|7       26|- PB3
//       PA2 -|8       25|- PA15
//       PA3 -|9       24|- PA14
//       PA4 -|10      23|- PA13
//       PA5 -|11      22|- PA12
//       PA6 -|12      21|- PA11
//       PA7 -|13      20|- PA10 (Reserved for RXD)
//       PB0 -|14      19|- PA9  (Reserved for TXD)
//       PB1 -|15      18|- PA8  (Measure the period at this pin)
//       VSS -|16      17|- VDD
//             ----------

void main(void)
{

	long int count;
	float T, f;
	float heart_rate;
	char buff[17];
	int i;
	int gender= 3; //(0 for f, 1 for m)
	int age;
	int age_flag = 4;
	
	
	Configure_Pins();
	LCD_4BIT();
	
	RCC->IOPENR |= 0x00000001; // peripheral clock enable for port A
	
	GPIOA->MODER &= ~(BIT16 | BIT17); // Make pin PA8 input
	// Activate pull up for pin PA8:
	GPIOA->PUPDR |= BIT16; 
	GPIOA->PUPDR &= ~(BIT17); 

	waitms(500); // Wait for putty to start.
	printf("Period measurement using the Systick free running counter.\r\n"
	      "Connect signal to PA8 (pin 18).\r\n");
	//        0123456789012345
	LCDprint("Heart Health",1,1);
	waitms(500);
	LCDprint(" ",1,1);
	waitms(500);
	
	LCDprint("Heart Health",1,1);
	waitms(500);
	LCDprint(" ",1,1);
	waitms(500);
	
	LCDprint("Heart Health",1,1);
	waitms(500);
	LCDprint(" ",1,1);
	waitms(500);
	
	LCDprint("Female or Male?",1,1);
	LCDprint("(F/M)",2,1);
	
	fflush(stdout); // GCC peculiarities: need to flush stdout to get string out without a '\n'
	egets_echo(buff, sizeof(buff));
	printf("\r\n");
		for(i=0; i<sizeof(buff); i++)
		{
			if(buff[i]=='\n') buff[i]=0;
			if(buff[i]=='\r') buff[i]=0;
		}
	LCDprint(buff, 2, 1);
	
	LCDprint("Age?",1,1);
	
	fflush(stdout); // GCC peculiarities: need to flush stdout to get string out without a '\n'
	egets_echo(buff, sizeof(buff));
	printf("\r\n");
		for(i=0; i<sizeof(buff); i++)
		{
			if(buff[i]=='\n') buff[i]=0;
			if(buff[i]=='\r') buff[i]=0;
		}
	LCDprint(buff, 2, 1);
	age = buff - '0';
	if(age < 18) age_flag = 0;
	else if(age > 60) age_flag = 2;
	else age_flag = 1;
	
	if ((gender != 3) && (age_flag != 4)) {
		
		while(1)
		{
			count=GetPeriod(1);
			
			if(count>0)
			{
				T=(count*1.0)/(F_CPU); // Since we have the time of 100 periods, we need to divide by 100
				f=1.0/T;
				heart_rate = 60/T;
				
				printf("\r\nT=%.2fms, count=%d            \r", T*1000, count);
				printf("\r\nHeart Rate = %.2f BPM", heart_rate);
				sprintf(buff,"%.2f BPM", heart_rate);
				LCDprint(buff, 2, 1);
				
				
			}
			else
			{
				printf("NO SIGNAL                     \r");
			}
			
			
			fflush(stdout); // GCC printf wants a \n in order to send something.  If \n is not present, we fflush(stdout)
			waitms(200);
		}
	}
}
