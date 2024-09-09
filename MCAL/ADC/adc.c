/*
 * adc.c
 *
 *  Created on: Aug 19, 2024
 *      Author: mustafa
 */

#include "adc.h"
#include "pll.h"

volatile static uint32 adc0Res = 0 ;    /* PE2 */
volatile static uint32 adc1Res = 0 ;    /* PE3 */

volatile static uint8  flag0      =0 ;
volatile static uint8  flag1      =0 ;

void ADC0SS3_handler(void){
    adc0Res = ADC0_ADCSSFIFO3 ;
    flag0=1;
    ADC0_ADCISC= (1<<3)       ;
}

void ADC1SS3_handler(void){
    adc1Res = ADC1_ADCSSFIFO3 ;
    flag1=1;
    ADC1_ADCISC = (1<<3)          ;
}

void ADC_ModuleInit(void){
    NVIC_EN1_R |= (1 << 19);
    NVIC_EN0_R |= (1 << 17);
    //activate clock for ADC 1 and 2
    SYSCTL_RCGCADC_REG |= (0x03);

    SYSCTL_RCGCGPIO_REG|=(1<<4) ;

    GPIO_PORTE_AFSEL_REG|=((1<<2)|(1<<3)) ;
    /*Configure the AINx signals to be analog inputs by clearing the corresponding DEN bit in the
     *GPIO Digital Enable (GPIODEN) register (see page 682).*/

    GPIO_PORTE_DEN_REG &=~ ((1<<2)|(1<<3)) ;
    /*Disable the analog isolation circuit for all ADC input pins that are to be used by writing a 1 to
    the appropriate bits of the GPIOAMSEL register (see page 687) in the associated GPIO block.*/
    GPIO_PORTE_AMSEL_REG|=((1<<3)|(1<<2)) ;
    //don't change
    /*If required by the application, reconfigure the sample sequencer priorities in the ADCSSPRI
    register. The default configuration has Sample Sequencer 0 with the highest priority and Sample
    Sequencer 3 as the lowest priority.
     */

}

void ADC_SampleSeqInit(void){

    /* disable sample sequencer number 3 (which will be used) for both ADCS */
    ADC0_ADCACTSS &= 0xF7;
    ADC1_ADCACTSS &= 0xF7;

    /* Configure trigger event for sample sequencer to Always */
    ADC0_ADCEMUX |= ( (0x0)<<12);
    ADC1_ADCEMUX |= ( (0x0)<<12);

    /* Configure analog input channels for ADC0 and ADC1 */
    ADC0_ADCSSMUX3 = 0;   //Activate PE2 for ADC0
    ADC1_ADCSSMUX3 = 1;   //Activate PE3 for ADC1

    /*5. For each sample in the sample sequence, configure the sample control bits in the
       * corresponding nibble in the ADCSSCTLn register. When programming the last nibble,
       * ensure that the END bit is set. Failure to set the END bit causes unpredictable behavior.*/
    ADC0_ADCSSCTL3 |= 0x06;
    ADC1_ADCSSCTL3 |= 0x06;

    /* 6. If interrupts are to be used, set the corresponding MASK bit
     * in the ADCIM register.*/
    ADC0_ADCIM |= 0x8;
    ADC1_ADCIM |= 0x8;

    /* Enable the sample sequencer logic by setting the corresponding ASENn bit in the
     * ADCACTSS register.*/
    ADC0_ADCACTSS |= 0x08   ;
    ADC1_ADCACTSS |= 0x08;

    /* clear status bit by writing one into he corresponding bit position */
    ADC0_ADCISC= (1<<3) ;
    ADC1_ADCISC= (1<<3) ;
}


void ADC_Init(void){
    PLL_Init();

    ADC_ModuleInit();
    ADC_SampleSeqInit();
}

uint32 ADC0_readChannel (void){
    ADC0_ADCPSSI|=(1<<3) ;
    while(flag0==0);

    flag0=0;

    return  adc0Res ;
}

uint32 ADC1_readChannel (void){
    ADC1_ADCPSSI|=(1<<3) ;
    while(flag1==0);

    flag1=0;

    return  adc1Res ;
}
