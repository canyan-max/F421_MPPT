/**
 ******************************************************************************
 *@file               :   svc_charge_maintenance.c
 *@brief              :   Implement charge completion and maintenance CV policy.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_maintenance.h"
#include "svc_charge_cv.h"
#include "svc_charge_config.h"
/* define   -----------------------------------------------------------------*/
#define SVC_CHARGE_MAINTENANCE_DISCHARGE_EXIT_MA (200U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
uint32_t g_charge_full_confirm_tick_count;
uint8_t  g_charge_full_confirmed;
uint32_t g_charge_maintenance_recharge_tick_count;

/* private  functions  ------------------------------------------------------*/
static uint32_t delay_ms_to_ticks(uint32_t delay_ms)
{
    uint32_t ticks = delay_ms / SVC_CHARGE_PROCESS_PERIOD_MS;

    if(0U != delay_ms % SVC_CHARGE_PROCESS_PERIOD_MS)
    {
        ticks++;
    }
    return ticks;
}

/* exported functions -------------------------------------------------------*/
void svc_charge_maintenance_reset(void)
{
    g_charge_full_confirm_tick_count         = 0U;
    g_charge_full_confirmed                  = 0U;
    g_charge_maintenance_recharge_tick_count = 0U;
}

uint8_t
svc_charge_maintenance_update_completion(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config      = svc_charge_config_get();
    uint32_t                   confirm_ticks = delay_ms_to_ticks(
        p_config->completion.confirm_delay_ms);

    if(0U != g_charge_full_confirmed)
    {
        return 1U;
    }
    if(NULL == p_value ||
       (p_config->voltage_target_mv - p_config->completion.voltage_margin_mv) >
           p_value->battery_voltage_mv ||
       -(int32_t)p_config->completion.discharge_tolerance_ma >
           p_value->battery_current_ma ||
       (0 < p_value->battery_current_ma &&
        p_config->completion.termination_current_ma <
            (uint32_t)p_value->battery_current_ma))
    {
        svc_charge_maintenance_reset();
        return 0U;
    }
    if(g_charge_full_confirm_tick_count < confirm_ticks)
    {
        g_charge_full_confirm_tick_count++;
    }
    if(confirm_ticks <= g_charge_full_confirm_tick_count)
    {
        g_charge_full_confirmed = 1U;
    }
    return g_charge_full_confirmed;
}

platform_err_t
svc_charge_maintenance_calculate_start_duty(const svc_measure_value_t *p_value,
                                            uint16_t *p_duty_permille)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || NULL == p_duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(p_config->maintenance.voltage_target_mv < p_value->battery_voltage_mv)
    {
        return PLATFORM_ERR_BUSY;
    }
    return svc_charge_control_calculate_initial_duty(p_value, p_duty_permille);
}

platform_err_t
svc_charge_maintenance_calculate_cv(const svc_measure_value_t *p_value,
                                    uint16_t current_duty_permille,
                                    svc_charge_control_result_t *p_result,
                                    svc_charge_fault_t          *p_fault)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    uint32_t                   voltage_target_mv;
    platform_err_t             status;

    if(NULL == p_value || NULL == p_result || NULL == p_fault)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_fault = svc_charge_protection_update_maintenance_reverse(p_value);
    if(SVC_CHARGE_FAULT_NONE != *p_fault)
    {
        return PLATFORM_ERR_HW;
    }
    voltage_target_mv = p_config->maintenance.voltage_target_mv;
    status = svc_charge_control_calculate_cv(p_value, current_duty_permille,
                                             voltage_target_mv, p_result);
    if(PLATFORM_ERR_OK != status)
    {
        *p_fault = SVC_CHARGE_FAULT_DUTY_LIMIT;
        return status;
    }
    *p_fault = svc_charge_protection_update_input_loss(p_value,
                                                       p_result
                                                           ->duty_high_limited);
    return (SVC_CHARGE_FAULT_NONE == *p_fault) ? PLATFORM_ERR_OK
                                               : PLATFORM_ERR_HW;
}

uint8_t svc_charge_maintenance_update_recharge_request(
    const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config       = svc_charge_config_get();
    uint32_t                   recharge_ticks = delay_ms_to_ticks(
        p_config->maintenance.recharge_delay_ms);

    if(NULL == p_value || -(int32_t)SVC_CHARGE_MAINTENANCE_DISCHARGE_EXIT_MA <
                              p_value->battery_current_ma)
    {
        g_charge_maintenance_recharge_tick_count = 0U;
        return 0U;
    }
    if(g_charge_maintenance_recharge_tick_count < recharge_ticks)
    {
        g_charge_maintenance_recharge_tick_count++;
    }
    return (recharge_ticks <= g_charge_maintenance_recharge_tick_count) ? 1U
                                                                        : 0U;
}

/* end of file --------------------------------------------------------------*/
