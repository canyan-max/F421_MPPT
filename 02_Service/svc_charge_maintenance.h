/**
 ******************************************************************************
 *@file               :   svc_charge_maintenance.h
 *@brief              :   Define charge completion and maintenance CV policy.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_MAINTENANCE_H
#define SVC_CHARGE_MAINTENANCE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_charge_control.h"
#include "svc_charge_protection.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
void svc_charge_maintenance_reset(void);
uint8_t
svc_charge_maintenance_update_completion(const svc_measure_value_t *p_value);
platform_err_t
svc_charge_maintenance_calculate_start_duty(const svc_measure_value_t *p_value,
                                            uint16_t *p_duty_permille);
platform_err_t
svc_charge_maintenance_calculate_cv(const svc_measure_value_t *p_value,
                                    uint16_t current_duty_permille,
                                    svc_charge_control_result_t *p_result,
                                    svc_charge_fault_t          *p_fault);
uint8_t svc_charge_maintenance_update_recharge_request(
    const svc_measure_value_t *p_value);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_MAINTENANCE_H */
/* end of file --------------------------------------------------------------*/
