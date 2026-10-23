/**
 ******************************************************************************
 *@file               :   svc_charge_cc.c
 *@brief              :   Calculate constant-current duty adjustment.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_cc.h"
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
svc_charge_control_calculate_cc(const svc_measure_value_t *p_value,
                                uint16_t current_duty_permille,
                                uint32_t current_target_ma,
                                svc_charge_control_result_t *p_result)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || NULL == p_result ||
       p_config->current_deadband_ma >= current_target_ma ||
       p_config->current_target_ma < current_target_ma)
    {
        return PLATFORM_ERR_PARAM;
    }
    initialize_result(p_result, current_duty_permille);
    p_result->current_error_ma = (int32_t)current_target_ma -
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

/* end of file --------------------------------------------------------------*/
