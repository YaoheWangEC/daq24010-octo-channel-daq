/* add user code begin Header */
/**
  **************************************************************************
  * @file     wk_tmr.c
  * @brief    work bench config program
  **************************************************************************
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
/* add user code end Header */

/* Includes ------------------------------------------------------------------*/
#include "wk_tmr.h"

/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  init tmr3 function.
  * @param  none
  * @retval none
  */
void wk_tmr3_init(void)
{
  /* add user code begin tmr3_init 0 */

  /* add user code end tmr3_init 0 */


  /* add user code begin tmr3_init 1 */

  /* add user code end tmr3_init 1 */

  /* configure counter settings */
  tmr_cnt_dir_set(TMR3, TMR_COUNT_UP);
  tmr_clock_source_div_set(TMR3, TMR_CLOCK_DIV1);
  tmr_period_buffer_enable(TMR3, FALSE);
  tmr_base_init(TMR3, 999, 0);

  /* configure primary mode settings */
  tmr_sub_sync_mode_set(TMR3, FALSE);
  tmr_primary_mode_select(TMR3, TMR_PRIMARY_SEL_RESET);

  /* configure dma overflow */
  tmr_dma_request_enable(TMR3, TMR_OVERFLOW_DMA_REQUEST, TRUE);
  tmr_counter_enable(TMR3, TRUE);

  /* add user code begin tmr3_init 2 */

  /* add user code end tmr3_init 2 */
}

/**
  * @brief  init tmr11 function.
  * @param  none
  * @retval none
  */
void wk_tmr11_init(void)
{
  /* add user code begin tmr11_init 0 */

  /* add user code end tmr11_init 0 */

  /* add user code begin tmr11_init 1 */

  /* add user code end tmr11_init 1 */

  /* configure counter settings */
  tmr_cnt_dir_set(TMR11, TMR_COUNT_UP);
  tmr_clock_source_div_set(TMR11, TMR_CLOCK_DIV1);
  tmr_period_buffer_enable(TMR11, FALSE);
  tmr_base_init(TMR11, 999, 119);

  tmr_counter_enable(TMR11, TRUE);

  /* enable ovfien interrupt */
  tmr_interrupt_enable(TMR11, TMR_OVF_INT, TRUE);

  /* add user code begin tmr11_init 2 */

  /* add user code end tmr11_init 2 */
}

/* add user code begin 1 */

/* add user code end 1 */
