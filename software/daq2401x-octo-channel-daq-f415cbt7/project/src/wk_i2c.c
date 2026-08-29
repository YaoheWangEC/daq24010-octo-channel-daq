/* add user code begin Header */
/**
  **************************************************************************
  * @file     wk_i2c.c
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
#include "wk_i2c.h"

/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  init i2c2 function.
  * @param  none
  * @retval none
  */
void wk_i2c2_init(void)
{
  /* add user code begin i2c2_init 0 */

  /* add user code end i2c2_init 0 */

  gpio_init_type gpio_init_struct;

  gpio_default_para_init(&gpio_init_struct);

  /* add user code begin i2c2_init 1 */

  /* add user code end i2c2_init 1 */
  
  /* configure the SCL pin */
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_pins = GPIO_PINS_10;
  gpio_init(GPIOB, &gpio_init_struct);

  /* configure the SDA pin */
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_pins = GPIO_PINS_11;
  gpio_init(GPIOB, &gpio_init_struct);

  i2c_init(I2C2, I2C_FSMODE_DUTY_2_1, 100000);
  i2c_own_address1_set(I2C2, I2C_ADDRESS_MODE_7BIT, 0x0);
  i2c_ack_enable(I2C2, TRUE);
  i2c_clock_stretch_enable(I2C2, TRUE);
  i2c_general_call_enable(I2C2, FALSE);

  /* add user code begin i2c2_init 2 */

  /* add user code end i2c2_init 2 */

  i2c_enable(I2C2, TRUE);

  /* add user code begin i2c2_init 3 */

  /* add user code end i2c2_init 3 */
}

/* add user code begin 1 */

/* add user code end 1 */
