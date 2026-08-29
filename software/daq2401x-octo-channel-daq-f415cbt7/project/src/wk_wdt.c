/* add user code begin Header */
/**
  **************************************************************************
  * @file     wk_wdt.c
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
#include "wk_wdt.h"

/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  init wdt function.
  * @param  none
  * @retval none
  */
void wk_wdt_init(void)
{
  /* add user code begin wdt_init 0 */

  /* add user code end wdt_init 0 */

  wdt_register_write_enable(TRUE);
  wdt_divider_set(WDT_CLK_DIV_32);
  wdt_reload_value_set(0xFFF);
  wdt_counter_reload();

  /* if enabled, please feed the dog through wdt_counter_reload() function */
  //wdt_enable();

  /* add user code begin wdt_init 1 */
  wdt_enable();
  wdt_counter_reload();
  /* add user code end wdt_init 1 */
}

/* add user code begin 1 */

/* add user code end 1 */
