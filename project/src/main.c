/* add user code begin Header */
/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
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
#include "at32f421_wk_config.h"
#include "wk_adc.h"
#include "wk_i2c.h"
#include "wk_tmr.h"
#include "wk_usart.h"
#include "wk_dma.h"
#include "wk_gpio.h"
#include "wk_system.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "plat_adc.h"
#include "plat_gpio.h"
#include "board_resources.h"

/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */
/* Static DMA memory: 20 complete frames, 10 frames per half. */
volatile uint16_t adc_test_buffer[100];
volatile uint32_t adc_test_half_count;
volatile uint32_t adc_test_full_count;
volatile uint32_t adc_test_error_count;
volatile uint32_t adc_test_last_sample;
volatile platform_err_t adc_test_start_result;
volatile uint32_t adc_test_stage;
volatile uint32_t adc_test_dma_remaining;
volatile uint32_t adc_test_dma_status;
volatile uint32_t adc_test_adc_status;
volatile uint32_t adc_test_timer_count;

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */
/* Scope marker only: bounded ISR work, no deferred DMA pointer. */
void adc_test_callback(plat_adc_event_t event,
    const volatile uint16_t *samples, uint16_t count)
{
    if(event == PLAT_ADC_EVENT_ERROR || samples == 0 || count != 50U)
    {
        adc_test_error_count++;
        (void)plat_gpio_write(BOARD_GPIO_LED2, PLAT_GPIO_SET);
        return;
    }
    adc_test_last_sample = samples[count - 1U];
    if(event == PLAT_ADC_EVENT_HALF) adc_test_half_count++;
    else if(event == PLAT_ADC_EVENT_FULL) adc_test_full_count++;
    (void)plat_gpio_toggle(BOARD_GPIO_LED1);
}
/* add user code end 0 */

/**
  * @brief main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  /* add user code begin 1 */

  /* add user code end 1 */

  /* system clock config. */
  wk_system_clock_config();

  /* config periph clock. */
  wk_periph_clock_config();

  /* nvic config. */
  wk_nvic_config();

  /* timebase config for
     void wk_delay_ms(uint32_t delay); */
  wk_timebase_init();

  /* init gpio function. */
  wk_gpio_config();

  /* init adc1 function. */
  wk_adc1_init();

  /* init dma1 channel1 */
  wk_dma1_channel1_init();
  /* config dma channel transfer parameter */
  /* user need to modify define values DMAx_CHANNELy_XXX_BASE_ADDR 
     and DMAx_CHANNELy_BUFFER_SIZE in at32xxx_wk_config.h */
  wk_dma_channel_config(DMA1_CHANNEL1, 
                        (uint32_t)&ADC1->odt, 
                        DMA1_CHANNEL1_MEMORY_BASE_ADDR, 
                        DMA1_CHANNEL1_BUFFER_SIZE);
  dma_channel_enable(DMA1_CHANNEL1, TRUE);

  /* init tmr1 function. */
  wk_tmr1_init();

  /* init usart1 function. */
  wk_usart1_init();

  /* init usart2 function. */
  wk_usart2_init();

  /* init i2c1 function. */
  wk_i2c1_init();

  /* add user code begin 2 */
  adc_test_stage = 5U;
  /* add user code end 2 */

  while(1)
  {
    /* add user code begin 3 */
    /* Temporary liveness diagnostic, independent of ADC/DMA interrupts.
     * Not a data consumer or final scheduler; IRQs remain enabled. */
    adc_test_dma_remaining = dma_data_number_get(DMA1_CHANNEL1);
    adc_test_dma_status = DMA1->sts;
    adc_test_adc_status = ADC1->sts;
    adc_test_timer_count = tmr_counter_value_get(TMR1);
    (void)plat_gpio_toggle(BOARD_GPIO_LED3);
    wk_delay_ms(100);
    /* add user code end 3 */
  }
}

  /* add user code begin 4 */

/* add user code end 4 */
