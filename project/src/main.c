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
#include "wk_exint.h"
#include "wk_i2c.h"
#include "wk_tmr.h"
#include "wk_usart.h"
#include "wk_dma.h"
#include "wk_gpio.h"
#include "wk_system.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "svc_measure.h"
#include "svc_charge.h"
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
/* Startup shutdown and measurement initialization results for Watch. */
static volatile platform_err_t g_measure_init_status;
static volatile platform_err_t g_charge_init_status;
static volatile platform_err_t g_charge_start_status = PLATFORM_ERR_BUSY;
static uint32_t                g_charge_task_last_ms;

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */
static void app_charge_task_process(void);
/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */
static void app_charge_task_process(void)
{
    uint32_t now_ms = wk_timebase_get();

    if((uint32_t)(now_ms - g_charge_task_last_ms) <
       SVC_CHARGE_PROCESS_PERIOD_MS)
    {
        return;
    }
    /* Skip missed slots instead of running several control updates at once. */
    g_charge_task_last_ms = now_ms;
    svc_charge_process();
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

  /* add user code begin measure_init */
    /* Bind acquisition before TMR15 starts producing ADC triggers. */
    g_measure_init_status = svc_measure_init();
    if(PLATFORM_ERR_OK != g_measure_init_status)
    {
        dma_channel_enable(DMA1_CHANNEL1, FALSE);
    }
  /* add user code end measure_init */

  /* init tmr1 function. */
  wk_tmr1_init();
  /* add user code begin power_default_off */
    g_charge_init_status =
        svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_12V);
  /* add user code end power_default_off */

  /* init usart1 function. */
  wk_usart1_init();

  /* init usart2 function. */
  wk_usart2_init();

  /* init i2c1 function. */
  wk_i2c1_init();

  /* init tmr15 function. */
  wk_tmr15_init();

  /* init exint function. */
  wk_exint_config();

  /* add user code begin 2 */
  if(PLATFORM_ERR_OK == g_measure_init_status &&
     PLATFORM_ERR_OK == g_charge_init_status)
  {
      g_charge_start_status = svc_charge_start();
  }
  g_charge_task_last_ms = wk_timebase_get();
  /* add user code end 2 */

  while(1)
  {
      /* add user code begin 3 */
      svc_measure_process();
      app_charge_task_process();
      /* add user code end 3 */
  }
}

  /* add user code begin 4 */

/* add user code end 4 */
