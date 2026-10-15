/**
 ******************************************************************************
 *@file               :   bsp_power_path.h
 *@brief              :   Provide board charge and discharge switching
 * interfaces.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_POWER_PATH_H
#define BSP_POWER_PATH_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE (150U)
#define BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE (950U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context, after GPIO/TMR1 initialization. Charge duty outside the
 * published min/max range is rejected. The caller owns startup timing,
 * soft start and current regulation.
 * Start: force PWM off, enable PB12 input switch, load duty and enable PWM.
 * Stop: force both PWM pins low, then disable PB12. Sampling keeps running.
 * Bootstrap readiness and live-load switching require hardware verification. */
platform_err_t bsp_power_path_charge_start(uint16_t duty_permille);
/* Main context only, while the charge PWM is running. */
platform_err_t bsp_power_path_charge_set_duty(uint16_t duty_permille);
platform_err_t bsp_power_path_charge_stop(void);
/* PB11: zero requests off, nonzero requests on. */
platform_err_t bsp_power_path_discharge_set(uint8_t enabled);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_POWER_PATH_H */
/* end of file --------------------------------------------------------------*/
