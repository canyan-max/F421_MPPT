/**
 ***************************************************************************************************
 *@file               :   svc_charge_control.h
 *@brief              :   Calculate Buck charge duty and input availability.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef SVC_CHARGE_CONTROL_H
#define SVC_CHARGE_CONTROL_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_measure.h"

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/
#define SVC_CHARGE_START_DUTY_OFFSET_PERMILLE (10U)

/* typedef ---------------------------------------------------------------------------------------*/
typedef struct SVC_CHARGE_CONTROL_RESULT_T {
    uint16_t duty_permille;
    int32_t  current_error_ma;
    int32_t  voltage_error_mv;
    uint8_t  duty_changed;
    uint8_t  duty_high_limited;
} svc_charge_control_result_t;

/* variables -------------------------------------------------------------------------------------*/

/* function  -------------------------------------------------------------------------------------*/
/* Rounded Vbat/reference voltage ratio, not clamped to the PWM range.
 * Running Dmin uses saved startup Voc; initial duty uses sampled Vin.
 * Requires 0 < Vbat < reference. Not a reverse-current safety guarantee. */
platform_err_t svc_charge_control_calculate_duty_reference(const svc_measure_value_t *p_value,
                                                           uint32_t  reference_voltage_mv,
                                                           uint16_t *p_reference_permille);
/* Recalculate the rounded ratio using the explicit reference voltage.
 * PARAM rejects the duty; it never raises PWM to preserve a lower bound.
 * Missing battery voltage/reference cannot authorize power output. */
platform_err_t svc_charge_control_check_duty_reference(const svc_measure_value_t *p_value,
                                                       uint32_t  reference_voltage_mv,
                                                       uint16_t  duty_permille,
                                                       uint16_t *p_reference_permille);
/* Startup ratio plus offset, capped at the permitted PWM maximum. */
platform_err_t svc_charge_control_calculate_initial_duty(const svc_measure_value_t *p_value,
                                                         uint16_t *p_duty_permille);
uint8_t        svc_charge_control_is_wake_cv_required(const svc_measure_value_t *p_value);
uint8_t        svc_charge_control_is_input_voltage_sufficient(const svc_measure_value_t *p_value,
                                                              uint8_t allow_missing_battery);
#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_CONTROL_H */
/* end of file -----------------------------------------------------------------------------------*/
