/* add user code begin Header */
/**
  **************************************************************************
  * @file     at32f421_wk_config.h
  * @brief    header file of work bench config
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

/* define to prevent recursive inclusion -----------------------------------*/
#ifndef __AT32F421_WK_CONFIG_H
#define __AT32F421_WK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* includes -----------------------------------------------------------------------*/
#include "stdio.h"
#include "at32f421.h"

/* private includes -------------------------------------------------------------*/
/* add user code begin private includes */

/* add user code end private includes */

/* exported types -------------------------------------------------------------*/
/* add user code begin exported types */
/* Sampling storage is defined in main.c and persists for DMA lifetime. */
extern volatile uint16_t g_adc_dma_buffer[12];

/* add user code end exported types */

/* exported constants --------------------------------------------------------*/
/* add user code begin exported constants */

/* add user code end exported constants */

/* exported macro ------------------------------------------------------------*/
/* add user code begin exported macro */

/* add user code end exported macro */

/* add user code begin dma define */
/* user can only modify the dma define value */
#define DMA1_CHANNEL1_BUFFER_SIZE   (12U)
#define DMA1_CHANNEL1_MEMORY_BASE_ADDR   ((uint32_t)g_adc_dma_buffer)
//#define DMA1_CHANNEL1_PERIPHERAL_BASE_ADDR  0

//#define DMA1_CHANNEL2_BUFFER_SIZE   0
//#define DMA1_CHANNEL2_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL2_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL3_BUFFER_SIZE   0
//#define DMA1_CHANNEL3_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL3_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL4_BUFFER_SIZE   0
//#define DMA1_CHANNEL4_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL4_PERIPHERAL_BASE_ADDR   0

//#define DMA1_CHANNEL5_BUFFER_SIZE   0
//#define DMA1_CHANNEL5_MEMORY_BASE_ADDR   0
//#define DMA1_CHANNEL5_PERIPHERAL_BASE_ADDR   0
/* add user code end dma define */

/* Private defines -------------------------------------------------------------*/
#define E2P_SDA_PF0_PIN    GPIO_PINS_0
#define E2P_SDA_PF0_GPIO_PORT    GPIOF
#define E2P_SCL_PF1_PIN    GPIO_PINS_1
#define E2P_SCL_PF1_GPIO_PORT    GPIOF
#define OUT_CUR_PA0_PIN    GPIO_PINS_0
#define OUT_CUR_PA0_GPIO_PORT    GPIOA
#define IN_CUR_PA1_PIN    GPIO_PINS_1
#define IN_CUR_PA1_GPIO_PORT    GPIOA
#define BAT_VOL_PA4_PIN    GPIO_PINS_4
#define BAT_VOL_PA4_GPIO_PORT    GPIOA
#define VIN_VOL_PA5_PIN    GPIO_PINS_5
#define VIN_VOL_PA5_GPIO_PORT    GPIOA
#define NTC_PA6_PIN    GPIO_PINS_6
#define NTC_PA6_GPIO_PORT    GPIOA
#define OUT_CUR_ERR_PIN    GPIO_PINS_0
#define OUT_CUR_ERR_GPIO_PORT    GPIOB
#define BT_VDD_EN_PB10_PIN    GPIO_PINS_10
#define BT_VDD_EN_PB10_GPIO_PORT    GPIOB
#define LOADMOS_OUT_PB11_PIN    GPIO_PINS_11
#define LOADMOS_OUT_PB11_GPIO_PORT    GPIOB
#define SOL_MOS_OUT_PB12_PIN    GPIO_PINS_12
#define SOL_MOS_OUT_PB12_GPIO_PORT    GPIOB
#define RS485_TXEN_PF7_PIN    GPIO_PINS_7
#define RS485_TXEN_PF7_GPIO_PORT    GPIOF
#define LED1_PB3_PIN    GPIO_PINS_3
#define LED1_PB3_GPIO_PORT    GPIOB
#define LED2_PB4_PIN    GPIO_PINS_4
#define LED2_PB4_GPIO_PORT    GPIOB
#define LED3_PB5_PIN    GPIO_PINS_5
#define LED3_PB5_GPIO_PORT    GPIOB
#define RS485_TX_PB6_PIN    GPIO_PINS_6
#define RS485_TX_PB6_GPIO_PORT    GPIOB
#define RS485_RX_PB7_PIN    GPIO_PINS_7
#define RS485_RX_PB7_GPIO_PORT    GPIOB

/* exported functions ------------------------------------------------------- */
  /* system clock config. */
  void wk_system_clock_config(void);

  /* config periph clock. */
  void wk_periph_clock_config(void);

  /* nvic config. */
  void wk_nvic_config(void);

/* add user code begin exported functions */
/* Project assembly hook: after ADC/DMA/TMR1 init, before sampling starts. */
void app_startup_before_sampling(void);
/* add user code end exported functions */

#ifdef __cplusplus
}
#endif

#endif
