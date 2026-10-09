/* add user code begin Header */
/**
  **************************************************************************
  * @file     wk_adc.c
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
#include "wk_adc.h"

/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  init adc1 function.
  * @param  none
  * @retval none
  */
void wk_adc1_init(void)
{
  /* add user code begin adc1_init 0 */

  /* add user code end adc1_init 0 */

  gpio_init_type gpio_init_struct;
  adc_base_config_type adc_base_struct;

  gpio_default_para_init(&gpio_init_struct);

  /* add user code begin adc1_init 1 */

  /* add user code end adc1_init 1 */

  /* gpio--------------------------------------------------------------------*/
  /* configure the IN0 pin */
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = OUT_CUR_PA0_PIN;
  gpio_init(OUT_CUR_PA0_GPIO_PORT, &gpio_init_struct);

  /* configure the IN1 pin */
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = IN_CUR_PA1_PIN;
  gpio_init(IN_CUR_PA1_GPIO_PORT, &gpio_init_struct);

  /* configure the IN4 pin */
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = BAT_VOL_PA4_PIN;
  gpio_init(BAT_VOL_PA4_GPIO_PORT, &gpio_init_struct);

  /* configure the IN5 pin */
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = VIN_VOL_PA5_PIN;
  gpio_init(VIN_VOL_PA5_GPIO_PORT, &gpio_init_struct);

  /* configure the IN6 pin */
  gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
  gpio_init_struct.gpio_pins = NTC_PA6_PIN;
  gpio_init(NTC_PA6_GPIO_PORT, &gpio_init_struct);

  adc_reset(ADC1);
  crm_adc_clock_div_set(CRM_ADC_DIV_6);

  /* adc_settings----------------------------------------------------------- */
  adc_base_default_para_init(&adc_base_struct);
  adc_base_struct.sequence_mode = TRUE;
  adc_base_struct.repeat_mode = FALSE;
  adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
  adc_base_struct.ordinary_channel_length = 5;
  adc_base_config(ADC1, &adc_base_struct);

  /* adc_ordinary_conversionmode-------------------------------------------- */
  adc_ordinary_channel_set(ADC1, ADC_CHANNEL_0, 1, ADC_SAMPLETIME_13_5);
  adc_ordinary_channel_set(ADC1, ADC_CHANNEL_1, 2, ADC_SAMPLETIME_13_5);
  adc_ordinary_channel_set(ADC1, ADC_CHANNEL_4, 3, ADC_SAMPLETIME_13_5);
  adc_ordinary_channel_set(ADC1, ADC_CHANNEL_5, 4, ADC_SAMPLETIME_13_5);
  adc_ordinary_channel_set(ADC1, ADC_CHANNEL_6, 5, ADC_SAMPLETIME_13_5);

  /* When "ADCx_ORDINARY_TRIG_SOFTWARE" is selected, user can only use software trigger. \
  The software trigger function is adc_ordinary_software_trigger_enable(ADCx, TRUE); */
  adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_TMR1CH1, TRUE);

  adc_ordinary_part_mode_enable(ADC1, FALSE);

  adc_dma_mode_enable(ADC1, TRUE);
  /* add user code begin adc1_init 2 */

  /* add user code end adc1_init 2 */

  adc_enable(ADC1, TRUE);

  /* adc calibration-------------------------------------------------------- */
  adc_calibration_init(ADC1);
  while(adc_calibration_init_status_get(ADC1));
  adc_calibration_start(ADC1);
  while(adc_calibration_status_get(ADC1));

  /* add user code begin adc1_init 3 */

  /* add user code end adc1_init 3 */
}

/* add user code begin 1 */

/* add user code end 1 */
