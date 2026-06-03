/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
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

#include "at32f45x_board.h"
#include "at32f45x_clock.h"

/** @addtogroup AT32F455_periph_examples
  * @{
  */

/** @addtogroup 455_TRNG_generate_random_numbers TRNG_generate_random_numbers
  * @{
  */

__IO uint32_t time_cnt = 0;
__IO uint32_t random_number = 0;
/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  system_clock_config();
  at32_board_init();
  uart_print_init(115200);

  /* enable trng clock */
  crm_periph_clock_enable(CRM_TRNG_PERIPH_CLOCK, TRUE);
 
  /* configure trng interrupt */
  nvic_irq_enable(TRNG_IRQn, 1, 0);
  trng_interrupt_enable(TRNG_DTRIE_INT | TRNG_FIE_INT, TRUE);
 
  /* enable trng noise source */
  trng_enable(TRUE);

  while(1)
  {
    if(at32_button_press() == USER_BUTTON)
    {
      /* output random number */
      printf("random number: 0x%08x\r\n", random_number);
    }
  }
}

/**
  * @}
  */

/**
  * @}
  */
