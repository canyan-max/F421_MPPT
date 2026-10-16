/**
 ******************************************************************************
 *@file               :   svc_charge.h
 *@brief              :   Start the Buck power path after validated sampling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_H
#define SVC_CHARGE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_charge_config.h"
#include "svc_charge_protection.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_STATE_T
{
    SVC_CHARGE_STATE_OFF = 0U,
    SVC_CHARGE_STATE_WAIT_SAMPLE,
    SVC_CHARGE_STATE_START_SETTLE,
    SVC_CHARGE_STATE_WAKE_CV          = 3U,
    SVC_CHARGE_STATE_CONSTANT_CURRENT = 4U,
    SVC_CHARGE_STATE_CONSTANT_VOLTAGE = 5U,
    SVC_CHARGE_STATE_FAULT            = 6U,
    SVC_CHARGE_STATE_FULL_WAIT        = 7U,
    SVC_CHARGE_STATE_MAINTENANCE      = 8U,
} svc_charge_state_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Forces the charge path off and resets the startup state. */
platform_err_t svc_charge_init(svc_charge_battery_system_t system);
/* Arms one automatic start after the measurement service becomes ready. */
platform_err_t svc_charge_start(void);
/* Nonblocking processing. Call exactly once per SVC_CHARGE_PROCESS_PERIOD_MS.
 */
void svc_charge_process(void);
/* Stops charging. A subsequent start call is required to re-arm. */
platform_err_t     svc_charge_stop(void);
svc_charge_state_t svc_charge_get_state(void);
svc_charge_fault_t svc_charge_get_fault(void);
platform_err_t     svc_charge_get_status(void);
uint16_t           svc_charge_get_duty_permille(void);
/* Main context only. The complete configuration is validated then copied. */
platform_err_t svc_charge_set_temperature_config(
    const svc_charge_temperature_config_t *p_config);
platform_err_t
svc_charge_get_temperature_config(svc_charge_temperature_config_t *p_config);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_H */
/* end of file --------------------------------------------------------------*/
