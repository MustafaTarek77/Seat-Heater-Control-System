/*
 * pll.c
 *
 *  Created on: Aug 19, 2024
 *      Author: mustafa
 */
#include "pll.h"

/* configure the system to get its clock from the PLL with Frequency 80Mhz */
void PLL_Init(void)
{
    /* 1) Configure the system to use RCC2 for advanced features
          such as 400 MHz PLL and non-integer System Clock Divisor */
    SYSCTL_RCC2_REG |= SYSCTL_RCC2_USERCC2_MASK;

    /* 2) Bypass PLL while initializing, Don’t use PLL while initialization */
    SYSCTL_RCC2_REG |= SYSCTL_RCC2_BYPASS2_MASK;

    /* 3) Select the crystal value and oscillator source */
    SYSCTL_RCC_REG  &= ~SYSCTL_RCC_XTAL_MASK;     /* clear XTAL field */
    SYSCTL_RCC_REG  |= (SYSCTL_RCC_XTAL_16MHZ << SYSCTL_RCC_XTAL_BIT_POS);  /* Set the XTAL bits for 16 MHz crystal */

    SYSCTL_RCC2_REG &= ~SYSCTL_RCC2_OSCSRC2_MASK; /* clear oscillator source field (OSCSRC2 bits) */
    SYSCTL_RCC2_REG |= (SYSCTL_RCC2_OSCSRC2_MOSC << SYSCTL_RCC2_OSCSRC2_BIT_POS);  /* configure for main oscillator source */

    /* 4) Activate PLL by clearing PWRDN2 */
    SYSCTL_RCC2_REG &= ~SYSCTL_RCC2_PWRDN2_MASK;

    /* 5) Set the desired system divider and the system divider least significant bit */
    SYSCTL_RCC2_REG |= SYSCTL_RCC2_DIV400_MASK;  /* use 400 MHz PLL */

    SYSCTL_RCC2_REG  = (SYSCTL_RCC2_REG & ~SYSCTL_RCC2_SYSDIV2_MASK)        /* clear system clock divider field */
                       | (SYSDIV2_VALUE << SYSCTL_RCC2_SYSDIV2_BIT_POS);      /* configure for 10MHz clock */

    /* 6) Wait for the PLL to lock by polling PLLLRIS bit */
    while(!(SYSCTL_RIS_REG & SYSCTL_RIS_PLLLRIS_MASK));

    /* 7) Enable use of PLL by clearing BYPASS2 */
    SYSCTL_RCC2_REG &= ~SYSCTL_RCC2_BYPASS2_MASK;
}
