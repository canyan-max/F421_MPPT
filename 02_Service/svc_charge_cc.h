/**
 ******************************************************************************
 *@file               :   svc_charge_cc.h
 *@brief              :   Calculate constant-current duty adjustment.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_CC_H
#define SVC_CHARGE_CC_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "svc_charge_control.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/

platform_err_t
svc_charge_control_calculate_cc(const svc_measure_value_t *p_value,
                                uint16_t current_duty_permille,
                                uint32_t current_target_ma,
                                svc_charge_control_result_t *p_result);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_CC_H */
/* end of file --------------------------------------------------------------*/
