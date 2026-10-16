/**
 ******************************************************************************
 *@file               :   svc_charge_protection.h
 *@brief              :   Evaluate Buck charge faults and recovery timing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_PROTECTION_H
#define SVC_CHARGE_PROTECTION_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "svc_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_FAULT_T
{
    SVC_CHARGE_FAULT_NONE = 0U,
    SVC_CHARGE_FAULT_MEASUREMENT,
    SVC_CHARGE_FAULT_SAMPLE_TIMEOUT,
    SVC_CHARGE_FAULT_INPUT_VOLTAGE,
    SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE,
    SVC_CHARGE_FAULT_CHARGE_OVERCURRENT,
    SVC_CHARGE_FAULT_TEMPERATURE_SENSOR,
    SVC_CHARGE_FAULT_DUTY_LIMIT,
    SVC_CHARGE_FAULT_POWER_PATH,
    SVC_CHARGE_FAULT_TEMPERATURE_LOW,
    SVC_CHARGE_FAULT_TEMPERATURE_HIGH,
    SVC_CHARGE_FAULT_CONFIG,
} svc_charge_fault_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
void svc_charge_protection_reset(void);
svc_charge_fault_t
svc_charge_protection_check_immediate(const svc_measure_value_t *p_value);
svc_charge_fault_t
svc_charge_protection_check_wake(const svc_measure_value_t *p_value);
svc_charge_fault_t
svc_charge_protection_update_temperature(const svc_measure_value_t *p_value);
svc_charge_fault_t
svc_charge_protection_update_input_loss(const svc_measure_value_t *p_value,
                                        uint8_t duty_high_limited);
svc_charge_fault_t svc_charge_protection_get_temperature_condition(
    const svc_measure_value_t *p_value);
svc_charge_fault_t
svc_charge_protection_check_start_voltage(const svc_measure_value_t *p_value);
uint8_t svc_charge_protection_is_recovery_condition_met(
    svc_charge_fault_t fault, const svc_measure_value_t *p_value);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_PROTECTION_H */
/* end of file --------------------------------------------------------------*/
