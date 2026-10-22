/**
 ******************************************************************************
 *@file               :   svc_discharge_config.c
 *@brief              :   Store the independent 12/24 V discharge configuration.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_discharge_config.h"
/* define   -----------------------------------------------------------------*/

#if (0U == SVC_DISCHARGE_PROCESS_PERIOD_MS) ||                                 \
    (2000U < SVC_DISCHARGE_PROCESS_PERIOD_MS)
#error "Discharge period must be nonzero and no longer than the trip delay"
#endif
/* Round the configured cycle count up for non-divisor periods. */
#define SVC_DISCHARGE_DELAY_TICKS(ms) \
    (((ms) + SVC_DISCHARGE_PROCESS_PERIOD_MS - 1U) / \
     SVC_DISCHARGE_PROCESS_PERIOD_MS)
#define SVC_DISCHARGE_TRIP_TICKS (SVC_DISCHARGE_DELAY_TICKS(2000U))
#define SVC_DISCHARGE_RECOVER_TICKS (SVC_DISCHARGE_DELAY_TICKS(5000U))
#define SVC_DISCHARGE_OC_RECOVER_TICKS (SVC_DISCHARGE_DELAY_TICKS(10000U))
#define SVC_DISCHARGE_SHORT_RETRY_TICKS (SVC_DISCHARGE_DELAY_TICKS(30000U))
#define SVC_DISCHARGE_SAMPLE_TIMEOUT_TICKS (SVC_DISCHARGE_TRIP_TICKS)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static svc_discharge_config_t g_discharge_config;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
platform_err_t svc_discharge_config_init(svc_discharge_battery_system_t system)
{
    if(SVC_DISCHARGE_BATTERY_SYSTEM_12V != system &&
       SVC_DISCHARGE_BATTERY_SYSTEM_24V != system)
    {
        return PLATFORM_ERR_PARAM;
    }
    g_discharge_config.low_trip_deci_c     = -400;
    g_discharge_config.low_recover_deci_c  = -300;
    g_discharge_config.high_trip_deci_c    = 750;
    g_discharge_config.high_recover_deci_c = 650;
    g_discharge_config.voltage_trip_mv    = (SVC_DISCHARGE_BATTERY_SYSTEM_12V ==
                                             system)
                                                ? 10000U
                                                : 20000U;
    g_discharge_config.voltage_recover_mv = (SVC_DISCHARGE_BATTERY_SYSTEM_12V ==
                                             system)
                                                ? 12000U
                                                : 24000U;
    g_discharge_config.current_trip_ma    = 9000U;
    g_discharge_config.trip_ticks         = SVC_DISCHARGE_TRIP_TICKS;
    g_discharge_config.recover_ticks      = SVC_DISCHARGE_RECOVER_TICKS;
    g_discharge_config
        .overcurrent_recover_ticks       = SVC_DISCHARGE_OC_RECOVER_TICKS;
    g_discharge_config.short_retry_ticks = SVC_DISCHARGE_SHORT_RETRY_TICKS;
    g_discharge_config
        .sample_timeout_ticks = SVC_DISCHARGE_SAMPLE_TIMEOUT_TICKS;
    return PLATFORM_ERR_OK;
}

const svc_discharge_config_t *svc_discharge_config_get(void)
{
    return &g_discharge_config;
}

/* end of file --------------------------------------------------------------*/
