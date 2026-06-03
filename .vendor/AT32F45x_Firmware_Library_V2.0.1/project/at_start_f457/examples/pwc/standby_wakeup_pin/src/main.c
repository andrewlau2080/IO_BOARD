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

/** @addtogroup AT32F457_periph_examples
  * @{
  */

/** @addtogroup 457_PWC_standby_wakeup_pin PWC_standby_wakeup_pin
  * @{
  */

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  /* congfig the system clock */
  system_clock_config();

  /* init at start board */
  at32_board_init();

  /* config priority group */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  /* turn on the led light */
  at32_led_off(LED2);
  at32_led_off(LED3);
  at32_led_off(LED4);

  /* enable pwc clock */
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, TRUE);

  if(pwc_flag_get(PWC_STANDBY_FLAG) != RESET)
  {
    /* wakeup from standby */
    pwc_flag_clear(PWC_STANDBY_FLAG);
    at32_led_on(LED2);
  }

  if(pwc_flag_get(PWC_WAKEUP_PIN1_FLAG) != RESET)
  {
    /* wakeup event occurs */
    pwc_flag_clear(PWC_WAKEUP_PIN1_FLAG);
    at32_led_on(LED3);
  }

  at32_led_on(LED4);
  
  /*delay to check led status*/
  delay_ms(1000);
	
  /* wakeup pin1 polarity set */
  pwc_wakeup_pin_polarity_select(PWC_WAKEUP_PIN_1, PWC_RISING_EDGE_WAKEUP);

  /* enable wakeup pin1 */
  pwc_wakeup_pin_enable(PWC_WAKEUP_PIN_1, TRUE);
  
  /* enter standby mode */
  pwc_standby_mode_enter();

  while(1)
  {
  }
}

/**
  * @}
  */

/**
  * @}
  */
