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
#include "string.h"

/** @addtogroup AT32F455_periph_examples
  * @{
  */

/** @addtogroup 455_OTP_write_and_lock OTP_write_and_lock
  * @{
  */

#define TEST_BUFEER_SIZE                 32
#define TEST_OTP_ADDRESS_START           (OTP_DATA_BASE + 2 * 32)

uint8_t buffer_write[TEST_BUFEER_SIZE];
uint8_t buffer_read[TEST_BUFEER_SIZE];

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  uint32_t index=0;
  flash_status_type status = FLASH_OPERATE_DONE;
  system_clock_config();
  at32_board_init();
  /* fill buffer_write data to test */
  for(index = 0; index < TEST_BUFEER_SIZE; index++)
  {
    buffer_write[index] = index;
  }

  /* wait button press */
  while(at32_button_press()==NO_BUTTON);
  
  flash_unlock();
  /* write otp data */
  status = flash_otp_data_program(TEST_OTP_ADDRESS_START, buffer_write, TEST_BUFEER_SIZE);

  /* read otp data */
  memcpy(buffer_read, (uint8_t *)TEST_OTP_ADDRESS_START, TEST_BUFEER_SIZE);

  /* compare the buffer */
  if((memcmp(buffer_write, buffer_read, TEST_BUFEER_SIZE) == 0) && (status == FLASH_OPERATE_DONE))
  {
    /* lock otp data */
    status = flash_otp_lock_enable((TEST_OTP_ADDRESS_START - OTP_DATA_BASE) / 32);
    if(status == FLASH_OPERATE_DONE)
    {
      at32_led_on(LED2);
      at32_led_on(LED3);
      at32_led_on(LED4);
    }
  }

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
