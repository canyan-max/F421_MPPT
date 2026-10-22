/**
 ******************************************************************************
 *@file               :   bsp_led.h
 *@brief              :   Drive the three active-low board status LEDs.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_LED_H
#define BSP_LED_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context after GPIO init. Arguments are 0=off/1=on.
 * Charge=PB5, battery=PB3, load=PB4; LOW lights each LED.
 * Attempts all three writes and returns the first error. No power control. */
platform_err_t
bsp_led_set(uint8_t charge_on, uint8_t battery_on, uint8_t load_on);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_LED_H */
/* end of file --------------------------------------------------------------*/
