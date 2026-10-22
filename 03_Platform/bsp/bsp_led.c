/**
 ******************************************************************************
 *@file               :   bsp_led.c
 *@brief              :   Bind status LEDs to their active-low GPIO outputs.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "bsp_led.h"
#include "plat_gpio.h"
#include "board_resources.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
platform_err_t
bsp_led_set(uint8_t charge_on, uint8_t battery_on, uint8_t load_on)
{
    platform_err_t charge_status;
    platform_err_t battery_status;
    platform_err_t load_status;

    if(1U < charge_on || 1U < battery_on || 1U < load_on)
    {
        return PLATFORM_ERR_PARAM;
    }
    charge_status  = plat_gpio_write(BOARD_GPIO_LED3, (0U != charge_on)
                                                          ? PLAT_GPIO_RESET
                                                          : PLAT_GPIO_SET);
    battery_status = plat_gpio_write(BOARD_GPIO_LED1, (0U != battery_on)
                                                          ? PLAT_GPIO_RESET
                                                          : PLAT_GPIO_SET);
    load_status    = plat_gpio_write(BOARD_GPIO_LED2, (0U != load_on)
                                                          ? PLAT_GPIO_RESET
                                                          : PLAT_GPIO_SET);
    if(PLATFORM_ERR_OK != charge_status)
    {
        return charge_status;
    }
    return (PLATFORM_ERR_OK != battery_status) ? battery_status : load_status;
}

/* end of file --------------------------------------------------------------*/
