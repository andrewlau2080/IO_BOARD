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

/** @addtogroup 455_CAN_communication_mode CAN_communication_mode
  * @{
  */

/* macro definition about some parameters of can bit time */
#define BITTIME_DIV                      1
#define AC_BTS1_SIZE                     144
#define AC_BTS2_SIZE                     48
#define AC_RSAW_SIZE                     48
/* the calculation process of can_clk is as follows:
   can_clk = can_pclk/BITTIME_DIV = 192M/1 = 192M;
*/
/* the calculation process of nominal bit rate is as follows:
   ac_baudrate = can_clk/(AC_BTS1_SIZE + AC_BTS2_SIZE) = 192M/(144 + 48) = 1M;
   ac_samplepoint = AC_BTS1_SIZE/(AC_BTS1_SIZE + AC_BTS2_SIZE) = 144/192 = 75%;
*/
/* Secondary Sample Point(SSP) is suggested to set the FD_SSP_OFFSET equal to FD_BTS1_SIZE+1. */

/**
  *  @brief  can gpio config
  *  @param  none
  *  @retval none
  */
static void can_gpio_config(void)
{
  gpio_init_type gpio_init_struct;

  /* enable the gpio clock */
  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);

  /* configure the can tx, rx, stb pin */
  /* can_stb connect to the transceiver <stby> pin, default output low, keep the transceiver
     running in normal mode */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOD, &gpio_init_struct);
  gpio_init_struct.gpio_pins = GPIO_PINS_7;
  gpio_init(GPIOB, &gpio_init_struct);

  gpio_pin_mux_config(GPIOD, GPIO_PINS_SOURCE0, GPIO_MUX_9);
  gpio_pin_mux_config(GPIOD, GPIO_PINS_SOURCE1, GPIO_MUX_9);
  gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE7, GPIO_MUX_11);
}

/**
  *  @brief  can configiguration.
  *  @param  none
  *  @retval the result of can_configuration
  *          this parameter can be one of the following values:
  *          SUCCESS or ERROR
  */
error_status can_configuration(void)
{
  can_bittime_type can_bittime_struct;

  /* as specified in can protocol, the maximum allowable oscillator tolerance is 1%.
     the hick accuracy does not meet the clock requirements in can protocol. to guarantee normal
     communication, it is recommended to use hext as the system clock source. */
  if(crm_flag_get(CRM_HEXT_STABLE_FLAG) != SET)
  {
    return ERROR;
  }

  /* enable the can clock */
  crm_periph_clock_enable(CRM_CAN1_PERIPH_CLOCK, TRUE);
  crm_can_clock_select(CRM_CAN1, CRM_CAN_CLOCK_SOURCE_PCLK);

  can_software_reset(CAN1, TRUE);

  /* set can bit time */
  can_bittime_default_para_init(&can_bittime_struct);
  can_bittime_struct.bittime_div = BITTIME_DIV;
  can_bittime_struct.ac_bts1_size = AC_BTS1_SIZE;
  can_bittime_struct.ac_bts2_size = AC_BTS2_SIZE;
  can_bittime_struct.ac_rsaw_size = AC_RSAW_SIZE;
  can_bittime_set(CAN1, &can_bittime_struct);

  can_stb_transmit_mode_set(CAN1, CAN_STB_TRANSMIT_BY_FIFO);

  can_software_reset(CAN1, FALSE);

  can_retransmission_limit_set(CAN1, CAN_RE_TRANS_TIMES_UNLIMIT);
  can_rearbitration_limit_set(CAN1, CAN_RE_ARBI_TIMES_UNLIMIT);
  can_rxbuf_warning_set(CAN1, 3);
  can_mode_set(CAN1, CAN_MODE_COMMUNICATE);

  /* can interrupt config */
  nvic_irq_enable(CAN1_RX_IRQn, 0, 0);
  nvic_irq_enable(CAN1_ERR_IRQn, 0, 0);
  can_interrupt_enable(CAN1, CAN_RAFIE_INT|CAN_RFIE_INT|CAN_ROIE_INT|CAN_RIE_INT, TRUE);
  can_interrupt_enable(CAN1, CAN_BEIE_INT, TRUE);

  return SUCCESS;
}

/**
  *  @brief  can transmit data
  *  @param  none
  *  @retval none
  */
static void can_transmit_data(void)
{
  can_txbuf_type  can_txbuf_struct = {0};

  /* write the primary transmit buffer */
  can_txbuf_struct.id = 0x400;
  can_txbuf_struct.id_type = CAN_ID_STANDARD;
  can_txbuf_struct.frame_type = CAN_FRAME_DATA;
  can_txbuf_struct.data_length = CAN_DLC_BYTES_8;
  can_txbuf_struct.data[0] = 0x00;
  can_txbuf_struct.data[1] = 0x11;
  can_txbuf_struct.data[2] = 0x22;
  can_txbuf_struct.data[3] = 0x33;
  can_txbuf_struct.data[4] = 0x44;
  can_txbuf_struct.data[5] = 0x55;
  can_txbuf_struct.data[6] = 0x66;
  can_txbuf_struct.data[7] = 0x77;
  while(can_txbuf_write(CAN1, CAN_TXBUF_PTB, &can_txbuf_struct) != SUCCESS);

  /* transmit the primary transmit buffer */
  can_txbuf_transmit(CAN1, CAN_TRANSMIT_PTB);
  while(can_flag_get(CAN1, CAN_TPIF_FLAG) != SET);
  can_flag_clear(CAN1, CAN_TPIF_FLAG);
}

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  system_clock_config();
  at32_board_init();
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  can_gpio_config();
  if(can_configuration() == ERROR)
  {
    /* can clock initialization error */
    while(1)
    {
    }
  }

  while(1)
  {
    can_transmit_data();

    at32_led_toggle(LED4);

    delay_sec(1);
  }
}

/**
  * @}
  */

/**
  * @}
  */
