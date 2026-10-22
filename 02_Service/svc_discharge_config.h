/**
 ******************************************************************************
 *@file               :   svc_discharge_config.h
 *@brief              :   Define discharge profiles and timing parameters.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_DISCHARGE_CONFIG_H
#define SVC_DISCHARGE_CONFIG_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

#define SVC_DISCHARGE_PROCESS_PERIOD_MS (200U)

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_DISCHARGE_BATTERY_SYSTEM_T
{
    SVC_DISCHARGE_BATTERY_SYSTEM_12V = 0U,
    SVC_DISCHARGE_BATTERY_SYSTEM_24V,
    SVC_DISCHARGE_BATTERY_SYSTEM_NUM,
} svc_discharge_battery_system_t;

typedef struct SVC_DISCHARGE_CONFIG_T
{
    int16_t  low_trip_deci_c;
    int16_t  low_recover_deci_c;
    int16_t  high_recover_deci_c;
    int16_t  high_trip_deci_c;
    uint32_t voltage_trip_mv;
    uint32_t voltage_recover_mv;
    uint32_t current_trip_ma;
    uint16_t trip_ticks;
    uint16_t recover_ticks;
    uint16_t overcurrent_recover_ticks;
    uint16_t short_retry_ticks;
    uint16_t sample_timeout_ticks;
} svc_discharge_config_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
platform_err_t svc_discharge_config_init(svc_discharge_battery_system_t system);
const svc_discharge_config_t *svc_discharge_config_get(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_DISCHARGE_CONFIG_H */
/* end of file --------------------------------------------------------------*/
