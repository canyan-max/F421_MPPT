/**
 ***************************************************************************************************
 *@file               :   svc_charge_control.c
 *@brief              :   Calculate Buck charge duty and input availability.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* includes --------------------------------------------------------------------------------------*/
#include "svc_charge_control.h"
#include "svc_charge_config.h"
#include "bsp_power_path.h"
/* define   --------------------------------------------------------------------------------------*/

/* typedef ---------------------------------------------------------------------------------------*/

/* variables -------------------------------------------------------------------------------------*/

/* private  functions  ---------------------------------------------------------------------------*/
/* exported functions ----------------------------------------------------------------------------*/
platform_err_t svc_charge_control_calculate_duty_reference(const svc_measure_value_t *p_value,
                                                           uint32_t  reference_voltage_mv,
                                                           uint16_t *p_duty_permille) {
    const svc_charge_config_t *p_config       = svc_charge_config_get();
    uint64_t                   duty_numerator = 0U;
    uint64_t                   duty_permille  = 0U;

    if (NULL == p_value || NULL == p_duty_permille || 0U == p_value->battery_voltage_mv ||
        reference_voltage_mv <= p_value->battery_voltage_mv) {
        return PLATFORM_ERR_PARAM;
    }
    duty_numerator = (uint64_t)p_value->battery_voltage_mv * p_config->duty_scale +
                     reference_voltage_mv / 2U;
    duty_permille  = duty_numerator / reference_voltage_mv;
    if (UINT16_MAX < duty_permille) {
        return PLATFORM_ERR_PARAM;
    }
    *p_duty_permille = (uint16_t)duty_permille;
    return PLATFORM_ERR_OK;
}

platform_err_t svc_charge_control_check_duty_reference(const svc_measure_value_t *p_value,
                                                       uint32_t  reference_voltage_mv,
                                                       uint16_t  duty_permille,
                                                       uint16_t *p_reference_permille) {
    platform_err_t status = PLATFORM_ERR_OK;
    if (NULL == p_value || NULL == p_reference_permille || 0U == p_value->input_voltage_mv ||
        0U == p_value->battery_voltage_mv ||
        p_value->input_voltage_mv <= p_value->battery_voltage_mv ||
        BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > duty_permille ||
        BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < duty_permille) {
        return PLATFORM_ERR_PARAM;
    }
    /* Reuse the explicit voltage reference, never substitute loaded Vin. */
    status = svc_charge_control_calculate_duty_reference(p_value, reference_voltage_mv,
                                                         p_reference_permille);
    if (PLATFORM_ERR_OK != status) {
        *p_reference_permille = 0U;
        return status;
    }
    return (0U == *p_reference_permille || duty_permille < *p_reference_permille)
               ? PLATFORM_ERR_PARAM
               : PLATFORM_ERR_OK;
}

platform_err_t svc_charge_control_calculate_initial_duty(const svc_measure_value_t *p_value,
                                                         uint16_t *p_duty_permille) {
    uint16_t       reference_permille = 0U;
    platform_err_t status             = PLATFORM_ERR_OK;
    if (NULL == p_value || NULL == p_duty_permille) {
        return PLATFORM_ERR_PARAM;
    }
    status = svc_charge_control_calculate_duty_reference(p_value, p_value->input_voltage_mv,
                                                         &reference_permille);
    if (PLATFORM_ERR_OK != status) {
        return status;
    }
    if (BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > reference_permille ||
        BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < reference_permille) {
        return PLATFORM_ERR_PARAM;
    }
    if (BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE - reference_permille <
        SVC_CHARGE_START_DUTY_OFFSET_PERMILLE) {
        *p_duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE;
    } else {
        *p_duty_permille = reference_permille + SVC_CHARGE_START_DUTY_OFFSET_PERMILLE;
    }
    return PLATFORM_ERR_OK;
}

uint8_t svc_charge_control_is_wake_cv_required(const svc_measure_value_t *p_value) {
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if (NULL == p_value || 0U == p_value->input_voltage_mv) {
        return 0U;
    }
    return ((uint64_t)p_value->battery_voltage_mv * p_config->duty_scale <
            (uint64_t)p_value->input_voltage_mv * BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE)
               ? 1U
               : 0U;
}

uint8_t svc_charge_control_is_input_voltage_sufficient(const svc_measure_value_t *p_value,
                                                       uint8_t allow_missing_battery) {
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if (NULL == p_value || 0U == p_value->input_voltage_mv) {
        return 0U;
    }
    if (0U != allow_missing_battery) {
        return (p_config->voltage_target_mv < p_value->input_voltage_mv) ? 1U : 0U;
    }
    if (0U == p_value->battery_voltage_mv) {
        return 0U;
    }
    return ((uint64_t)p_value->battery_voltage_mv * p_config->duty_scale <
            (uint64_t)p_value->input_voltage_mv * BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE)
               ? 1U
               : 0U;
}

/* end of file -----------------------------------------------------------------------------------*/
