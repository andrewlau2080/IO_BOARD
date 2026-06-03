/**
  **************************************************************************
  * @file     at32f45x_int.c
  * @brief    main interrupt service routines.
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

/* includes ------------------------------------------------------------------*/
#include "at32f45x_int.h"
#include "at32f45x_board.h"

/** @addtogroup AT32F455_periph_examples
  * @{
  */

/** @addtogroup 455_CAN_transmit_status
  * @{
  */

/**
  * @brief  this function handles nmi exception.
  * @param  none
  * @retval none
  */
void NMI_Handler(void)
{
}

/**
  * @brief  this function handles hard fault exception.
  * @param  none
  * @retval none
  */
void HardFault_Handler(void)
{
  /* go to infinite loop when hard fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles memory manage exception.
  * @param  none
  * @retval none
  */
void MemManage_Handler(void)
{
  /* go to infinite loop when memory manage exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles bus fault exception.
  * @param  none
  * @retval none
  */
void BusFault_Handler(void)
{
  /* go to infinite loop when bus fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles usage fault exception.
  * @param  none
  * @retval none
  */
void UsageFault_Handler(void)
{
  /* go to infinite loop when usage fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles svcall exception.
  * @param  none
  * @retval none
  */
void SVC_Handler(void)
{
}

/**
  * @brief  this function handles debug monitor exception.
  * @param  none
  * @retval none
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  this function handles pendsv_handler exception.
  * @param  none
  * @retval none
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  this function handles systick handler.
  * @param  none
  * @retval none
  */
void SysTick_Handler(void)
{
}

/**
  *  @brief  can1 interrupt function tx
  *  @param  none
  *  @retval none
  */
void CAN1_TX_IRQHandler(void)
{
  can_transmit_status_type transmit_status_struct;
  
  /* transmission of the primary transmit buffer be completed */
  if(can_interrupt_flag_get(CAN1, CAN_TPIF_FLAG) != RESET)
  {
    can_flag_clear(CAN1, CAN_TPIF_FLAG);
    /* get the status of the can frame transmissions */
    can_transmit_status_get(CAN1, &transmit_status_struct);
    printf("transmit status:\r\n");
    printf("current: handle=%u tstat=%u\r\n", transmit_status_struct.current_handle, transmit_status_struct.current_tstat);
    printf("final:   handle=%u tstat=%u\r\n", transmit_status_struct.final_handle, transmit_status_struct.final_tstat);
    printf("\r\n");
    /* verify whether the transmission has completed successfully */
    if(transmit_status_struct.final_tstat == CAN_TSTAT_TRANSMITTED)
      at32_led_toggle(LED3);
    else
      at32_led_toggle(LED2);
  }
}

/**
  *  @brief  can1 interrupt function rx
  *  @param  none
  *  @retval none
  */
void CAN1_RX_IRQHandler(void)
{
  can_rxbuf_type can_rxbuf_struct;

  /* rx_buffer had data be received */
  if(can_interrupt_flag_get(CAN1, CAN_RIF_FLAG) != RESET)
  {
    can_flag_clear(CAN1, CAN_RIF_FLAG);
    while(ERROR != can_rxbuf_read(CAN1, &can_rxbuf_struct))
    {

    }
  }
}

/**
  *  @brief  can1 interrupt function error
  *  @param  none
  *  @retval none
  */
void CAN1_ERR_IRQHandler(void)
{
  /* bus error */
  if(can_interrupt_flag_get(CAN1, CAN_BEIF_FLAG) != RESET)
  {
    can_flag_clear(CAN1, CAN_BEIF_FLAG);
  }
}

/**
  * @}
  */

/**
  * @}
  */


