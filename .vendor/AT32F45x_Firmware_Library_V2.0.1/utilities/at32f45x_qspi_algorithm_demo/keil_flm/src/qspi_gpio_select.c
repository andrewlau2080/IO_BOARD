/**
  **************************************************************************
  * @file     qspi1_gpio_select.c
  * @brief    device algorithm for new device flash
  **************************************************************************
  *
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include "at32f45x.h"

/** @addtogroup UTILITIES_examples
  * @{
  */

/** @addtogroup QSPI_flash_algorithm_for_keil
  * @{
  */

//<<< Use Configuration Wizard in Context Menu >>>
// <h>QSPI1 GPIO select
//   <i>Note: qspi1 gpio select
void qspi_gpio_slect(void)
{
  // <o>  QSPI1 CS pin
  //  <i> choose which  pin as qspi1 cs signal
  //  <0=> PA2
  //  <1=> PB6
  //  <2=> PB9
  //  <3=> PB10
  //  <4=> PC11
  //  <5=> PG6
  #define QSPI_CS_PIN_SELECT     5
  #if (QSPI_CS_PIN_SELECT == 0)
    #define GPIO_CS_PORT_VALUE  GPIOA
    #define GPIO_CS_CLOCK_VALUE  0U
    #define GPIO_CS_PIN_VALUE    2U
    #define GPIO_CS_MUX_VALUE    10U
  #elif (QSPI_CS_PIN_SELECT == 1)
    #define GPIO_CS_PORT_VALUE  GPIOB
    #define GPIO_CS_CLOCK_VALUE  1U
    #define GPIO_CS_PIN_VALUE    6U
    #define GPIO_CS_MUX_VALUE    10U
  #elif (QSPI_CS_PIN_SELECT == 2)
    #define GPIO_CS_PORT_VALUE  GPIOB
    #define GPIO_CS_CLOCK_VALUE  1U
    #define GPIO_CS_PIN_VALUE    9U
    #define GPIO_CS_MUX_VALUE    10U
  #elif (QSPI_CS_PIN_SELECT == 3)
    #define GPIO_CS_PORT_VALUE  GPIOB
    #define GPIO_CS_CLOCK_VALUE  1U
    #define GPIO_CS_PIN_VALUE    10U
    #define GPIO_CS_MUX_VALUE    9U
  #elif (QSPI_CS_PIN_SELECT == 4)
    #define GPIO_CS_PORT_VALUE  GPIOC
    #define GPIO_CS_CLOCK_VALUE  2U
    #define GPIO_CS_PIN_VALUE    11U
    #define GPIO_CS_MUX_VALUE    9U
  #elif (QSPI_CS_PIN_SELECT == 5)
    #define GPIO_CS_PORT_VALUE  GPIOG
    #define GPIO_CS_CLOCK_VALUE  6U
    #define GPIO_CS_PIN_VALUE    6U
    #define GPIO_CS_MUX_VALUE    10U
  #endif
  // <o>  QSPI1 SCK pin
  //  <i> choose which  pin as qspi1 sck signal
  //  <0=> PB1
  //  <1=> PB2
  //  <2=> PB4
  //  <3=> PD3
  //  <4=> PF10
  #define QSPI_SCK_PIN_SELECT     4
  #if (QSPI_SCK_PIN_SELECT == 0)
    #define GPIO_SCK_PORT_VALUE  GPIOB
    #define GPIO_SCK_CLOCK_VALUE  1U
    #define GPIO_SCK_PIN_VALUE    1U
    #define GPIO_SCK_MUX_VALUE    9U
  #elif (QSPI_SCK_PIN_SELECT == 1)
    #define GPIO_SCK_PORT_VALUE  GPIOB
    #define GPIO_SCK_CLOCK_VALUE  1U
    #define GPIO_SCK_PIN_VALUE    2U
    #define GPIO_SCK_MUX_VALUE    9U
  #elif (QSPI_SCK_PIN_SELECT == 2)
    #define GPIO_SCK_PORT_VALUE  GPIOB
    #define GPIO_SCK_CLOCK_VALUE  1U
    #define GPIO_SCK_PIN_VALUE    4U
    #define GPIO_SCK_MUX_VALUE    10U
  #elif (QSPI_SCK_PIN_SELECT == 3)
    #define GPIO_SCK_PORT_VALUE  GPIOD
    #define GPIO_SCK_CLOCK_VALUE  3U
    #define GPIO_SCK_PIN_VALUE    3U
    #define GPIO_SCK_MUX_VALUE    9U
  #elif (QSPI_SCK_PIN_SELECT == 4)
    #define GPIO_SCK_PORT_VALUE  GPIOF
    #define GPIO_SCK_CLOCK_VALUE  5U
    #define GPIO_SCK_PIN_VALUE    10U
    #define GPIO_SCK_MUX_VALUE    9U
  #endif
  // <o>  QSPI1 IO0 pin
  //  <i> choose which  pin as qspi1 io0 signal
  //  <0=> PA6
  //  <1=> PB0
  //  <2=> PB5
  //  <3=> PB11
  //  <4=> PC9
  //  <5=> PD11
  //  <6=> PF8
  //  <7=> PH2
  #define QSPI_IO0_PIN_SELECT     6
  #if (QSPI_IO0_PIN_SELECT == 0)
    #define GPIO_IO0_PORT_VALUE  GPIOA
    #define GPIO_IO0_CLOCK_VALUE  0U
    #define GPIO_IO0_PIN_VALUE    6U
    #define GPIO_IO0_MUX_VALUE    10U
  #elif (QSPI_IO0_PIN_SELECT == 1)
    #define GPIO_IO0_PORT_VALUE  GPIOB
    #define GPIO_IO0_CLOCK_VALUE  1U
    #define GPIO_IO0_PIN_VALUE    0U
    #define GPIO_IO0_MUX_VALUE    10U
  #elif (QSPI_IO0_PIN_SELECT == 2)
    #define GPIO_IO0_PORT_VALUE  GPIOB
    #define GPIO_IO0_CLOCK_VALUE  1U
    #define GPIO_IO0_PIN_VALUE    5U
    #define GPIO_IO0_MUX_VALUE    10U
  #elif (QSPI_IO0_PIN_SELECT == 3)
    #define GPIO_IO0_PORT_VALUE  GPIOB
    #define GPIO_IO0_CLOCK_VALUE  1U
    #define GPIO_IO0_PIN_VALUE    11U
    #define GPIO_IO0_MUX_VALUE    10U
  #elif (QSPI_IO0_PIN_SELECT == 4)
    #define GPIO_IO0_PORT_VALUE  GPIOC
    #define GPIO_IO0_CLOCK_VALUE  2U
    #define GPIO_IO0_PIN_VALUE    9U
    #define GPIO_IO0_MUX_VALUE    9U
  #elif (QSPI_IO0_PIN_SELECT == 5)
    #define GPIO_IO0_PORT_VALUE  GPIOD
    #define GPIO_IO0_CLOCK_VALUE  3U
    #define GPIO_IO0_PIN_VALUE    11U
    #define GPIO_IO0_MUX_VALUE    9U
  #elif (QSPI_IO0_PIN_SELECT == 6)
    #define GPIO_IO0_PORT_VALUE  GPIOF
    #define GPIO_IO0_CLOCK_VALUE  5U
    #define GPIO_IO0_PIN_VALUE    8U
    #define GPIO_IO0_MUX_VALUE    10U
  #elif (QSPI_IO0_PIN_SELECT == 7)
    #define GPIO_IO0_PORT_VALUE  GPIOH
    #define GPIO_IO0_CLOCK_VALUE  7U
    #define GPIO_IO0_PIN_VALUE    2U
    #define GPIO_IO0_MUX_VALUE    10U
  #endif
  // <o>  QSPI1 IO1 pin
  //  <i> choose which  pin as qspi1 io1 signal
  //  <0=> PA7
  //  <1=> PB7
  //  <2=> PB10
  //  <3=> PC10
  //  <4=> PD12
  //  <5=> PF9
  //  <6=> PH3
  #define QSPI_IO1_PIN_SELECT     5
  #if (QSPI_IO1_PIN_SELECT == 0)
    #define GPIO_IO1_PORT_VALUE  GPIOA
    #define GPIO_IO1_CLOCK_VALUE  0U
    #define GPIO_IO1_PIN_VALUE    7U
    #define GPIO_IO1_MUX_VALUE    10U
  #elif (QSPI_IO1_PIN_SELECT == 1)
    #define GPIO_IO1_PORT_VALUE  GPIOB
    #define GPIO_IO1_CLOCK_VALUE  1U
    #define GPIO_IO1_PIN_VALUE    7U
    #define GPIO_IO1_MUX_VALUE    10U
  #elif (QSPI_IO1_PIN_SELECT == 2)
    #define GPIO_IO1_PORT_VALUE  GPIOB
    #define GPIO_IO1_CLOCK_VALUE  1U
    #define GPIO_IO1_PIN_VALUE    10U
    #define GPIO_IO1_MUX_VALUE    10U
  #elif (QSPI_IO1_PIN_SELECT == 3)
    #define GPIO_IO1_PORT_VALUE  GPIOC
    #define GPIO_IO1_CLOCK_VALUE  2U
    #define GPIO_IO1_PIN_VALUE    10U
    #define GPIO_IO1_MUX_VALUE    9U
  #elif (QSPI_IO1_PIN_SELECT == 4)
    #define GPIO_IO1_PORT_VALUE  GPIOD
    #define GPIO_IO1_CLOCK_VALUE  3U
    #define GPIO_IO1_PIN_VALUE    12U
    #define GPIO_IO1_MUX_VALUE    9U
  #elif (QSPI_IO1_PIN_SELECT == 5)
    #define GPIO_IO1_PORT_VALUE  GPIOF
    #define GPIO_IO1_CLOCK_VALUE  5U
    #define GPIO_IO1_PIN_VALUE    9U
    #define GPIO_IO1_MUX_VALUE    10U
  #elif (QSPI_IO1_PIN_SELECT == 6)
    #define GPIO_IO1_PORT_VALUE  GPIOH
    #define GPIO_IO1_CLOCK_VALUE  7U
    #define GPIO_IO1_PIN_VALUE    3U
    #define GPIO_IO1_MUX_VALUE    10U
  #endif
  // <o>  QSPI1 IO2 pin
  //  <i> choose which  pin as qspi1 io2 signal
  //  <0=> PA6
  //  <1=> PA15
  //  <2=> PC4
  //  <3=> PC8
  //  <4=> PE2
  //  <5=> PF7
  //  <6=> PG9
  #define QSPI_IO2_PIN_SELECT     5
  #if (QSPI_IO2_PIN_SELECT == 0)
    #define GPIO_IO2_PORT_VALUE  GPIOA
    #define GPIO_IO2_CLOCK_VALUE  0U
    #define GPIO_IO2_PIN_VALUE    6U
    #define GPIO_IO2_MUX_VALUE    13U
  #elif (QSPI_IO2_PIN_SELECT == 1)
    #define GPIO_IO2_PORT_VALUE  GPIOA
    #define GPIO_IO2_CLOCK_VALUE  0U
    #define GPIO_IO2_PIN_VALUE    15U
    #define GPIO_IO2_MUX_VALUE    10U
  #elif (QSPI_IO2_PIN_SELECT == 2)
    #define GPIO_IO2_PORT_VALUE  GPIOC
    #define GPIO_IO2_CLOCK_VALUE  2U
    #define GPIO_IO2_PIN_VALUE    4U
    #define GPIO_IO2_MUX_VALUE    10U
  #elif (QSPI_IO2_PIN_SELECT == 3)
    #define GPIO_IO2_PORT_VALUE  GPIOC
    #define GPIO_IO2_CLOCK_VALUE  2U
    #define GPIO_IO2_PIN_VALUE    8U
    #define GPIO_IO2_MUX_VALUE    9U
  #elif (QSPI_IO2_PIN_SELECT == 4)
    #define GPIO_IO2_PORT_VALUE  GPIOE
    #define GPIO_IO2_CLOCK_VALUE  4U
    #define GPIO_IO2_PIN_VALUE    2U
    #define GPIO_IO2_MUX_VALUE    9U
  #elif (QSPI_IO2_PIN_SELECT == 5)
    #define GPIO_IO2_PORT_VALUE  GPIOF
    #define GPIO_IO2_CLOCK_VALUE  5U
    #define GPIO_IO2_PIN_VALUE    7U
    #define GPIO_IO2_MUX_VALUE    9U
  #elif (QSPI_IO2_PIN_SELECT == 6)
    #define GPIO_IO2_PORT_VALUE  GPIOG
    #define GPIO_IO2_CLOCK_VALUE  6U
    #define GPIO_IO2_PIN_VALUE    9U
    #define GPIO_IO2_MUX_VALUE    9U
  #endif
  // <o>  QSPI1 IO3 pin
  //  <i> choose which  pin as qspi1 io3 signal
  //  <0=> PA1
  //  <1=> PB3
  //  <2=> PC5
  //  <3=> PD13
  //  <4=> PF6
  //  <5=> PG14
  #define QSPI_IO3_PIN_SELECT     4
  #if (QSPI_IO3_PIN_SELECT == 0)
    #define GPIO_IO3_PORT_VALUE  GPIOA
    #define GPIO_IO3_CLOCK_VALUE  0U
    #define GPIO_IO3_PIN_VALUE    1U
    #define GPIO_IO3_MUX_VALUE    9U
  #elif (QSPI_IO3_PIN_SELECT == 1)
    #define GPIO_IO3_PORT_VALUE  GPIOB
    #define GPIO_IO3_CLOCK_VALUE  1U
    #define GPIO_IO3_PIN_VALUE    3U
    #define GPIO_IO3_MUX_VALUE    10U
  #elif (QSPI_IO3_PIN_SELECT == 2)
    #define GPIO_IO3_PORT_VALUE  GPIOC
    #define GPIO_IO3_CLOCK_VALUE  2U
    #define GPIO_IO3_PIN_VALUE    5U
    #define GPIO_IO3_MUX_VALUE    10U
  #elif (QSPI_IO3_PIN_SELECT == 3)
    #define GPIO_IO3_PORT_VALUE  GPIOD
    #define GPIO_IO3_CLOCK_VALUE  3U
    #define GPIO_IO3_PIN_VALUE    13U
    #define GPIO_IO3_MUX_VALUE    9U
  #elif (QSPI_IO3_PIN_SELECT == 4)
    #define GPIO_IO3_PORT_VALUE  GPIOF
    #define GPIO_IO3_CLOCK_VALUE  5U
    #define GPIO_IO3_PIN_VALUE    6U
    #define GPIO_IO3_MUX_VALUE    9U
  #elif (QSPI_IO3_PIN_SELECT == 5)
    #define GPIO_IO3_PORT_VALUE  GPIOG
    #define GPIO_IO3_CLOCK_VALUE  6U
    #define GPIO_IO3_PIN_VALUE    14U
    #define GPIO_IO3_MUX_VALUE    9U
  #endif
  /* enable the qspi clock */
  CRM->ahben3 |= (uint32_t)(1<<1);
  
  /* enable the gpio pin clock */
  CRM->ahben1 |= (uint32_t)(1<<GPIO_CS_CLOCK_VALUE);
  CRM->ahben1 |= (uint32_t)(1<<GPIO_SCK_CLOCK_VALUE);
  CRM->ahben1 |= (uint32_t)(1<<GPIO_IO0_CLOCK_VALUE);
  CRM->ahben1 |= (uint32_t)(1<<GPIO_IO1_CLOCK_VALUE);
  CRM->ahben1 |= (uint32_t)(1<<GPIO_IO2_CLOCK_VALUE);
  CRM->ahben1 |= (uint32_t)(1<<GPIO_IO3_CLOCK_VALUE);
  
  /* configure the gpio */  
  GPIO_CS_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_CS_PIN_VALUE));
  GPIO_CS_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_CS_PIN_VALUE));
  GPIO_CS_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_CS_PIN_VALUE));
#if (GPIO_CS_PIN_VALUE < 8)
  GPIO_CS_PORT_VALUE->muxl |= (uint32_t)(GPIO_CS_MUX_VALUE<<(4 * GPIO_CS_PIN_VALUE));
#else
  GPIO_CS_PORT_VALUE->muxh |= (uint32_t)(GPIO_CS_MUX_VALUE<<(4 *(GPIO_CS_PIN_VALUE - 8)));
#endif

  GPIO_SCK_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_SCK_PIN_VALUE));
  GPIO_SCK_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_SCK_PIN_VALUE));
  GPIO_SCK_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_SCK_PIN_VALUE));
#if (GPIO_SCK_PIN_VALUE < 8)
  GPIO_SCK_PORT_VALUE->muxl |= (uint32_t)(GPIO_SCK_MUX_VALUE<<(4 * GPIO_SCK_PIN_VALUE));
#else
  GPIO_SCK_PORT_VALUE->muxh |= (uint32_t)(GPIO_SCK_MUX_VALUE<<(4 *(GPIO_SCK_PIN_VALUE - 8)));
#endif

  GPIO_IO0_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_IO0_PIN_VALUE));
  GPIO_IO0_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_IO0_PIN_VALUE));
  GPIO_IO0_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_IO0_PIN_VALUE));
#if (GPIO_IO0_PIN_VALUE < 8)
  GPIO_IO0_PORT_VALUE->muxl |= (uint32_t)(GPIO_IO0_MUX_VALUE<<(4 * GPIO_IO0_PIN_VALUE));
#else
  GPIO_IO0_PORT_VALUE->muxh |= (uint32_t)(GPIO_IO0_MUX_VALUE<<(4 *(GPIO_IO0_PIN_VALUE - 8)));
#endif
  
  GPIO_IO1_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_IO1_PIN_VALUE));  
  GPIO_IO1_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_IO1_PIN_VALUE));
  GPIO_IO1_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_IO1_PIN_VALUE));
#if (GPIO_IO1_PIN_VALUE < 8)
  GPIO_IO1_PORT_VALUE->muxl |= (uint32_t)(GPIO_IO1_MUX_VALUE<<(4 * GPIO_IO1_PIN_VALUE));
#else
  GPIO_IO1_PORT_VALUE->muxh |= (uint32_t)(GPIO_IO1_MUX_VALUE<<(4 *(GPIO_IO1_PIN_VALUE - 8)));
#endif

  GPIO_IO2_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_IO2_PIN_VALUE));
  GPIO_IO2_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_IO2_PIN_VALUE));
  GPIO_IO2_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_IO2_PIN_VALUE));
#if (GPIO_IO2_PIN_VALUE < 8)
  GPIO_IO2_PORT_VALUE->muxl |= (uint32_t)(GPIO_IO2_MUX_VALUE<<(4 * GPIO_IO2_PIN_VALUE));
#else
  GPIO_IO2_PORT_VALUE->muxh |= (uint32_t)(GPIO_IO2_MUX_VALUE<<(4 *(GPIO_IO2_PIN_VALUE - 8)));
#endif
  
  GPIO_IO3_PORT_VALUE->cfgr &= ~(uint32_t)(3<<(2 * GPIO_IO3_PIN_VALUE));  
  GPIO_IO3_PORT_VALUE->cfgr |= (uint32_t)(2<<(2 * GPIO_IO3_PIN_VALUE));
  GPIO_IO3_PORT_VALUE->odrvr |= (uint32_t)(1<<(2 * GPIO_IO3_PIN_VALUE));
#if (GPIO_IO3_PIN_VALUE < 8)
  GPIO_IO3_PORT_VALUE->muxl |= (uint32_t)(GPIO_IO3_MUX_VALUE<<(4 * GPIO_IO3_PIN_VALUE));
#else
  GPIO_IO3_PORT_VALUE->muxh |= (uint32_t)(GPIO_IO3_MUX_VALUE<<(4 *(GPIO_IO3_PIN_VALUE - 8)));
#endif
}
// </h>
//<<< end of configuration section >>>

/**
  * @}
  */

/**
  * @}
  */
