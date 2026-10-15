/**
 ******************************************************************************
 *@file               :   plat_pwm.h
 *@brief              :   Provide explicit low levels on Buck control outputs.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PLAT_PWM_H
#define PLAT_PWM_H

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
/* TMR1 CH3/CH3C (PA10/PB1). Main context only.
 * Call after generated TMR1 initialization; no concurrent PWM writers.
 * Caller owns start/stop sequencing and duty validation/limits.
 * No range checks or clamping are performed here. */
/* Disable PWM and force both pins low; timer and sampling keep running. */
platform_err_t plat_pwm_outputs_off(void);
/* Load initial duty and restart the PWM counter before enabling output.
 * Calling again restarts PWM; first-pulse timing requires verification. */
platform_err_t plat_pwm_start(uint16_t duty_permille);
/* Permille units: 500 means 50%. Can preload while stopped.
 * Uses the configured period/preload; does not enable output. */
platform_err_t plat_pwm_set_duty_permille(uint16_t duty_permille);
#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PLAT_PWM_H */
