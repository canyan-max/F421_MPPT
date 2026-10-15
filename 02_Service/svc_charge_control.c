/**
 ******************************************************************************
 *@file               :   svc_charge_control.c
 *@brief              :   Calculate Buck charge duty and input availability.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_control.h"
#include "svc_charge_config.h"
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/
static void initialize_result(svc_charge_control_result_t *p_result,
                              uint16_t current_duty_permille)
{
    p_result->duty_permille     = current_duty_permille;
    p_result->current_error_ma  = 0;
    p_result->voltage_error_mv  = 0;
    p_result->duty_changed      = 0U;
    p_result->duty_high_limited = 0U;
}

/* exported functions -------------------------------------------------------*/
platform_err_t
svc_charge_control_calculate_initial_duty(const svc_measure_value_t *p_value,
                                          uint16_t *p_duty_permille)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    uint64_t                   duty_numerator;
    uint64_t                   duty_permille;

    if(NULL == p_value || NULL == p_duty_permille ||
       0U == p_value->battery_voltage_mv ||
       p_value->input_voltage_mv <= p_value->battery_voltage_mv)
    {
        return PLATFORM_ERR_PARAM;
    }
    duty_numerator = (uint64_t)p_value->battery_voltage_mv *
                         p_config->duty_scale +
                     p_value->input_voltage_mv / 2U;
    duty_permille  = duty_numerator / p_value->input_voltage_mv;
    if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_duty_permille = (uint16_t)duty_permille;
    return PLATFORM_ERR_OK;
}

uint8_t
svc_charge_control_is_wake_cv_required(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || 0U == p_value->input_voltage_mv)
    {
        return 0U;
    }
    return ((uint64_t)p_value->battery_voltage_mv * p_config->duty_scale <
            (uint64_t)p_value->input_voltage_mv *
                BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE)
               ? 1U
               : 0U;
}

uint8_t svc_charge_control_is_input_voltage_sufficient(
    const svc_measure_value_t *p_value, uint8_t allow_missing_battery)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || 0U == p_value->input_voltage_mv)
    {
        return 0U;
    }
    if(0U != allow_missing_battery)
    {
        return (p_config->voltage_target_mv < p_value->input_voltage_mv) ? 1U
                                                                         : 0U;
    }
    if(0U == p_value->battery_voltage_mv)
    {
        return 0U;
    }
    return ((uint64_t)p_value->battery_voltage_mv * p_config->duty_scale <
            (uint64_t)p_value->input_voltage_mv *
                BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE)
               ? 1U
               : 0U;
}

platform_err_t
svc_charge_control_calculate_cc(const svc_measure_value_t *p_value,
                                uint16_t current_duty_permille,
                                svc_charge_control_result_t *p_result)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || NULL == p_result)
    {
        return PLATFORM_ERR_PARAM;
    }
    initialize_result(p_result, current_duty_permille);
    p_result->current_error_ma = (int32_t)p_config->current_target_ma -
                                 (int32_t)p_value->input_current_ma;
    if((int32_t)p_config->current_deadband_ma < p_result->current_error_ma)
    {
        if((BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE -
            p_config->control_step_permille) < current_duty_permille)
        {
            p_result->duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE;
            p_result->duty_high_limited = 1U;
            return PLATFORM_ERR_OK;
        }
        p_result->duty_permille += p_config->control_step_permille;
        p_result->duty_changed = 1U;
    }
    else if(-(int32_t)p_config->current_deadband_ma >
            p_result->current_error_ma)
    {
        if((BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE +
            p_config->control_step_permille) > current_duty_permille)
        {
            return PLATFORM_ERR_PARAM;
        }
        p_result->duty_permille -= p_config->control_step_permille;
        p_result->duty_changed = 1U;
    }
    return PLATFORM_ERR_OK;
}

platform_err_t
svc_charge_control_calculate_cv(const svc_measure_value_t *p_value,
                                uint16_t current_duty_permille,
                                svc_charge_control_result_t *p_result)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || NULL == p_result)
    {
        return PLATFORM_ERR_PARAM;
    }
    initialize_result(p_result, current_duty_permille);
    p_result->current_error_ma = (int32_t)p_config->current_target_ma -
                                 (int32_t)p_value->input_current_ma;
    p_result->voltage_error_mv = (int32_t)p_config->voltage_target_mv -
                                 (int32_t)p_value->battery_voltage_mv;
    if(-(int32_t)p_config->voltage_deadband_mv > p_result->voltage_error_mv ||
       -(int32_t)p_config->current_deadband_ma > p_result->current_error_ma)
    {
        if((BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE +
            p_config->control_step_permille) > current_duty_permille)
        {
            return PLATFORM_ERR_PARAM;
        }
        p_result->duty_permille -= p_config->control_step_permille;
        p_result->duty_changed = 1U;
    }
    else if((int32_t)p_config->voltage_deadband_mv <
                p_result->voltage_error_mv &&
            (int32_t)p_config->current_deadband_ma < p_result->current_error_ma)
    {
        if((BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE -
            p_config->control_step_permille) < current_duty_permille)
        {
            p_result->duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE;
            p_result->duty_high_limited = 1U;
            return PLATFORM_ERR_OK;
        }
        p_result->duty_permille += p_config->control_step_permille;
        p_result->duty_changed = 1U;
    }
    return PLATFORM_ERR_OK;
}

/* end of file --------------------------------------------------------------*/
