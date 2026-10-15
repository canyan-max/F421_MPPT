/**
 ******************************************************************************
 *@file               :   svc_charge_config.h
 *@brief              :   Define and select Buck charge configuration values.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_CONFIG_H
#define SVC_CHARGE_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SVC_CHARGE_PROCESS_PERIOD_MS (10U)

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_BATTERY_SYSTEM_T
{
    SVC_CHARGE_BATTERY_SYSTEM_12V = 0U,
    SVC_CHARGE_BATTERY_SYSTEM_24V,
    SVC_CHARGE_BATTERY_SYSTEM_NUM,
} svc_charge_battery_system_t;

typedef struct SVC_CHARGE_TEMPERATURE_CONFIG_T
{
    int16_t  low_trip_deci_c;
    int16_t  low_recover_deci_c;
    int16_t  high_recover_deci_c;
    int16_t  high_trip_deci_c;
    uint32_t trip_delay_ms;
} svc_charge_temperature_config_t;

typedef struct SVC_CHARGE_RECOVERY_CONFIG_T
{
    uint32_t normal_delay_ms;
    uint32_t overcurrent_delay_ms;
} svc_charge_recovery_config_t;

typedef struct SVC_CHARGE_CONFIG_T
{
    uint32_t                        voltage_target_mv;
    uint32_t                        voltage_deadband_mv;
    uint32_t                        cv_exit_margin_mv;
    uint32_t                        current_target_ma;
    uint32_t                        current_limit_ma;
    uint32_t                        current_deadband_ma;
    uint16_t                        duty_scale;
    uint16_t                        control_step_permille;
    uint16_t                        sample_timeout_ticks;
    uint16_t                        start_settle_ticks;
    svc_charge_temperature_config_t temperature;
    svc_charge_recovery_config_t    recovery;
} svc_charge_config_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
platform_err_t svc_charge_config_init(svc_charge_battery_system_t system);
const svc_charge_config_t *svc_charge_config_get(void);
platform_err_t             svc_charge_config_validate(void);
platform_err_t             svc_charge_config_set_temperature(
    const svc_charge_temperature_config_t *p_config);
platform_err_t
svc_charge_config_get_temperature(svc_charge_temperature_config_t *p_config);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_CONFIG_H */
/* end of file --------------------------------------------------------------*/
