/**
 ******************************************************************************
 *@file               :   svc_led.h
 *@brief              :   Indicate charge, battery and load operating status.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_LED_H
#define SVC_LED_H

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
/* After GPIO init: all LEDs off until an operating status is available. */
platform_err_t svc_led_init(void);
/* Main context after charge/discharge updates, about every 200 ms.
 * Reads status only, never changes charge/discharge permission or faults.
 * LED write failures are reported and retried by the next call. */
platform_err_t svc_led_process(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_LED_H */
/* end of file --------------------------------------------------------------*/
