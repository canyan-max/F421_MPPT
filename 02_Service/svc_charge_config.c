/**
 ******************************************************************************
 *@file               :   svc_charge_config.c
 *@brief              :   Store validated Buck charge configuration values.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_config.h"
/* define   -----------------------------------------------------------------*/
#define SVC_CHARGE_VOLTAGE_12V_TARGET_MV (14400U)
#define SVC_CHARGE_VOLTAGE_24V_TARGET_MV (29200U)
#define SVC_CHARGE_MAINTENANCE_12V_TARGET_MV (14100U)
#define SVC_CHARGE_MAINTENANCE_24V_TARGET_MV (28000U)
#define SVC_CHARGE_MAINTENANCE_12V_RECHARGE_MV (13800U)
#define SVC_CHARGE_MAINTENANCE_24V_RECHARGE_MV (27600U)
#define SVC_CHARGE_MAINTENANCE_RECHARGE_DELAY_MS (5000U)
#define SVC_CHARGE_VOLTAGE_DEADBAND_MV (20U)
#define SVC_CHARGE_CV_EXIT_MARGIN_MV (100U)
#define SVC_CHARGE_CURRENT_TARGET_MA (4000U)
#define SVC_CHARGE_CURRENT_LIMIT_MA (12000U)
#define SVC_CHARGE_CURRENT_DEADBAND_MA (20U)
#define SVC_CHARGE_DUTY_SCALE (1000U)
#define SVC_CHARGE_CONTROL_STEP_PERMILLE (1U)
#define SVC_CHARGE_SAMPLE_TIMEOUT_TICKS (2U) /* 20 ms. */
#define SVC_CHARGE_START_SETTLE_TICKS (2U)  /* 20 ms. */
#define SVC_CHARGE_TEMP_LOW_TRIP_DECI_C (0)
#define SVC_CHARGE_TEMP_LOW_RECOVER_DECI_C (50)
#define SVC_CHARGE_TEMP_HIGH_RECOVER_DECI_C (550)
#define SVC_CHARGE_TEMP_HIGH_TRIP_DECI_C (650)
#define SVC_CHARGE_TEMP_TRIP_DELAY_MS (2000U)
#define SVC_CHARGE_RECOVERY_NORMAL_DELAY_MS (5000U)
#define SVC_CHARGE_RECOVERY_OVERCURRENT_DELAY_MS (20000U)
#define SVC_CHARGE_COMPLETION_VOLTAGE_MARGIN_MV (200U)
#define SVC_CHARGE_COMPLETION_CURRENT_MA (500U)
#define SVC_CHARGE_COMPLETION_DISCHARGE_TOLERANCE_MA (500U)
#define SVC_CHARGE_COMPLETION_CONFIRM_DELAY_MS (10000U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static svc_charge_config_t g_charge_config;

/* private  functions  ------------------------------------------------------*/
static platform_err_t
validate_temperature_config(const svc_charge_temperature_config_t *p_config)
{
    if(NULL == p_config || 0U == p_config->trip_delay_ms ||
       p_config->low_trip_deci_c >= p_config->low_recover_deci_c ||
       p_config->low_recover_deci_c >= p_config->high_recover_deci_c ||
       p_config->high_recover_deci_c >= p_config->high_trip_deci_c)
    {
        return PLATFORM_ERR_PARAM;
    }
    return PLATFORM_ERR_OK;
}

static platform_err_t validate_config(const svc_charge_config_t *p_config)
{
    if(NULL == p_config || 0U == p_config->voltage_target_mv ||
       p_config->voltage_deadband_mv >= p_config->cv_exit_margin_mv ||
       p_config->voltage_target_mv <= p_config->cv_exit_margin_mv ||
       0U == p_config->current_target_ma ||
       p_config->current_target_ma <= p_config->current_deadband_ma ||
       p_config->current_limit_ma <= p_config->current_target_ma ||
       p_config->current_limit_ma <= p_config->current_deadband_ma ||
       0U == p_config->duty_scale || 0U == p_config->control_step_permille ||
       0U == p_config->sample_timeout_ticks ||
       0U == p_config->start_settle_ticks ||
       0U == p_config->recovery.normal_delay_ms ||
       p_config->recovery.normal_delay_ms >
           p_config->recovery.overcurrent_delay_ms ||
       0U == p_config->completion.voltage_margin_mv ||
       p_config->voltage_target_mv <= p_config->completion.voltage_margin_mv ||
       0U == p_config->completion.termination_current_ma ||
       p_config->current_target_ma <=
           p_config->completion.termination_current_ma ||
       0U == p_config->completion.discharge_tolerance_ma ||
       p_config->current_target_ma <=
           p_config->completion.discharge_tolerance_ma ||
       0U == p_config->completion.confirm_delay_ms ||
       0U == p_config->maintenance.voltage_target_mv ||
       0U == p_config->maintenance.recharge_voltage_mv ||
       p_config->maintenance.recharge_voltage_mv >=
           p_config->maintenance.voltage_target_mv ||
       0U == p_config->maintenance.recharge_delay_ms ||
       p_config->voltage_target_mv < p_config->maintenance.voltage_target_mv)
    {
        return PLATFORM_ERR_PARAM;
    }
    return validate_temperature_config(&p_config->temperature);
}

static void load_common_config(svc_charge_config_t *p_config)
{
    p_config->voltage_deadband_mv         = SVC_CHARGE_VOLTAGE_DEADBAND_MV;
    p_config->cv_exit_margin_mv           = SVC_CHARGE_CV_EXIT_MARGIN_MV;
    p_config->current_target_ma           = SVC_CHARGE_CURRENT_TARGET_MA;
    p_config->current_limit_ma            = SVC_CHARGE_CURRENT_LIMIT_MA;
    p_config->current_deadband_ma         = SVC_CHARGE_CURRENT_DEADBAND_MA;
    p_config->duty_scale                  = SVC_CHARGE_DUTY_SCALE;
    p_config->control_step_permille       = SVC_CHARGE_CONTROL_STEP_PERMILLE;
    p_config->sample_timeout_ticks        = SVC_CHARGE_SAMPLE_TIMEOUT_TICKS;
    p_config->start_settle_ticks          = SVC_CHARGE_START_SETTLE_TICKS;
    p_config->temperature.low_trip_deci_c = SVC_CHARGE_TEMP_LOW_TRIP_DECI_C;
    p_config->temperature
        .low_recover_deci_c = SVC_CHARGE_TEMP_LOW_RECOVER_DECI_C;
    p_config->temperature
        .high_recover_deci_c = SVC_CHARGE_TEMP_HIGH_RECOVER_DECI_C;
    p_config->temperature.high_trip_deci_c = SVC_CHARGE_TEMP_HIGH_TRIP_DECI_C;
    p_config->temperature.trip_delay_ms    = SVC_CHARGE_TEMP_TRIP_DELAY_MS;
    p_config->recovery.normal_delay_ms = SVC_CHARGE_RECOVERY_NORMAL_DELAY_MS;
    p_config->recovery
        .overcurrent_delay_ms = SVC_CHARGE_RECOVERY_OVERCURRENT_DELAY_MS;
    p_config->completion
        .voltage_margin_mv = SVC_CHARGE_COMPLETION_VOLTAGE_MARGIN_MV;
    p_config->completion
        .termination_current_ma = SVC_CHARGE_COMPLETION_CURRENT_MA;
    p_config->completion
        .discharge_tolerance_ma = SVC_CHARGE_COMPLETION_DISCHARGE_TOLERANCE_MA;
    p_config->completion
        .confirm_delay_ms = SVC_CHARGE_COMPLETION_CONFIRM_DELAY_MS;
    p_config->maintenance
        .recharge_delay_ms = SVC_CHARGE_MAINTENANCE_RECHARGE_DELAY_MS;
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_charge_config_init(svc_charge_battery_system_t system)
{
    if(SVC_CHARGE_BATTERY_SYSTEM_NUM <= system)
    {
        return PLATFORM_ERR_PARAM;
    }
    load_common_config(&g_charge_config);
    g_charge_config.voltage_target_mv = (SVC_CHARGE_BATTERY_SYSTEM_12V ==
                                         system)
                                            ? SVC_CHARGE_VOLTAGE_12V_TARGET_MV
                                            : SVC_CHARGE_VOLTAGE_24V_TARGET_MV;
    g_charge_config.maintenance
        .voltage_target_mv = (SVC_CHARGE_BATTERY_SYSTEM_12V == system)
                                 ? SVC_CHARGE_MAINTENANCE_12V_TARGET_MV
                                 : SVC_CHARGE_MAINTENANCE_24V_TARGET_MV;
    g_charge_config.maintenance
        .recharge_voltage_mv = (SVC_CHARGE_BATTERY_SYSTEM_12V == system)
                                   ? SVC_CHARGE_MAINTENANCE_12V_RECHARGE_MV
                                   : SVC_CHARGE_MAINTENANCE_24V_RECHARGE_MV;
    return validate_config(&g_charge_config);
}

const svc_charge_config_t *svc_charge_config_get(void)
{
    return &g_charge_config;
}

platform_err_t svc_charge_config_validate(void)
{
    return validate_config(&g_charge_config);
}

platform_err_t svc_charge_config_set_temperature(
    const svc_charge_temperature_config_t *p_config)
{
    platform_err_t status = validate_temperature_config(p_config);

    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    g_charge_config.temperature = *p_config;
    return PLATFORM_ERR_OK;
}

platform_err_t
svc_charge_config_get_temperature(svc_charge_temperature_config_t *p_config)
{
    if(NULL == p_config)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_config = g_charge_config.temperature;
    return PLATFORM_ERR_OK;
}

/* end of file --------------------------------------------------------------*/
