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
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/
#define SVC_CHARGE_MAINTENANCE_DISCHARGE_EXIT_MA (200U)
/* Match the existing low-output threshold. The bounded 300 ms startup window
 * is a board-trial value, not a verified hardware settling requirement. */
#define SVC_CHARGE_MAINTENANCE_SUPPLY_MIN_MA (100U)
#define SVC_CHARGE_MAINTENANCE_BUILD_TIMEOUT_MS (300U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
uint32_t                       g_charge_full_confirm_tick_count;
uint8_t                        g_charge_full_confirmed;
uint32_t                       g_charge_maintenance_recharge_tick_count;
svc_charge_maintenance_phase_t g_charge_maintenance_phase;
uint32_t                       g_charge_maintenance_build_ticks;
/* Cumulative timeout diagnostic survives ordinary fault/reset processing. */
uint32_t        g_charge_maintenance_build_timeout_count;
uint32_t        g_charge_maintenance_low_output_wait_count;
static uint32_t g_maintenance_supply_confirm_ticks;
static uint32_t g_maintenance_build_step_ticks;
uint8_t         g_charge_cv_load_active = 1U;
uint32_t        g_charge_cv_no_load_ticks;

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
void svc_charge_maintenance_idle(void)
{
    g_charge_maintenance_phase         = SVC_CHARGE_MAINTENANCE_PHASE_IDLE;
    g_charge_maintenance_build_ticks   = 0U;
    g_maintenance_supply_confirm_ticks = 0U;
    g_maintenance_build_step_ticks     = 0U;
}

void svc_charge_maintenance_start(void)
{
    svc_charge_maintenance_idle();
    g_charge_maintenance_phase = SVC_CHARGE_MAINTENANCE_PHASE_BUILD;
}

void svc_charge_maintenance_wait(void)
{
    svc_charge_maintenance_idle();
    g_charge_maintenance_phase = SVC_CHARGE_MAINTENANCE_PHASE_WAIT;
}

void svc_charge_maintenance_reset(void)
{
    svc_charge_maintenance_idle();
    g_charge_full_confirm_tick_count         = 0U;
    g_charge_full_confirmed                  = 0U;
    g_charge_maintenance_recharge_tick_count = 0U;
    g_charge_cv_load_active                  = 1U;
    g_charge_cv_no_load_ticks                = 0U;
}

uint8_t
svc_charge_maintenance_update_idle_request(const svc_measure_value_t *p_value)
{
    uint32_t confirm_ticks = delay_ms_to_ticks(SVC_CHARGE_NO_LOAD_CONFIRM_MS);

    if(NULL == p_value || 0U == g_charge_full_confirmed)
    {
        g_charge_cv_load_active   = 1U;
        g_charge_cv_no_load_ticks = 0U;
        return 0U;
    }
    if(SVC_CHARGE_LOAD_ON_MA <= p_value->output_current_ma)
    {
        g_charge_cv_load_active   = 1U;
        g_charge_cv_no_load_ticks = 0U;
    }
    else if(SVC_CHARGE_LOAD_OFF_MA >= p_value->output_current_ma)
    {
        if(g_charge_cv_no_load_ticks < confirm_ticks)
        {
            g_charge_cv_no_load_ticks++;
        }
        if(confirm_ticks <= g_charge_cv_no_load_ticks)
        {
            g_charge_cv_load_active = 0U;
        }
    }
    else
    {
        /* Keep hysteresis; stopping needs continuous samples below OFF. */
        g_charge_cv_no_load_ticks = 0U;
    }
    return (0U == g_charge_cv_load_active) ? 1U : 0U;
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
    uint32_t restart_voltage_mv = p_config->maintenance.voltage_target_mv;

    if(NULL == p_value || NULL == p_duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(SVC_CHARGE_MAINTENANCE_PHASE_WAIT == g_charge_maintenance_phase)
    {
        if(restart_voltage_mv <= p_config->voltage_deadband_mv)
        {
            return PLATFORM_ERR_PARAM;
        }
        restart_voltage_mv -= p_config->voltage_deadband_mv;
    }
    if(restart_voltage_mv < p_value->battery_voltage_mv)
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

    if(NULL == p_value || NULL == p_result || NULL == p_fault ||
       BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > current_duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < current_duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_fault = SVC_CHARGE_FAULT_NONE;
    if(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase)
    {
        g_charge_maintenance_build_ticks++;
        g_maintenance_build_step_ticks++;
        if(SVC_CHARGE_MAINTENANCE_SUPPLY_MIN_MA <= p_value->input_current_ma)
        {
            g_maintenance_supply_confirm_ticks++;
        }
        else
        {
            g_maintenance_supply_confirm_ticks = 0U;
        }
        if(p_config->start_settle_ticks <= g_maintenance_supply_confirm_ticks)
        {
            g_charge_maintenance_phase = SVC_CHARGE_MAINTENANCE_PHASE_RUNNING;
        }
        else if(delay_ms_to_ticks(SVC_CHARGE_MAINTENANCE_BUILD_TIMEOUT_MS) <=
                g_charge_maintenance_build_ticks)
        {
            if(UINT32_MAX != g_charge_maintenance_build_timeout_count)
            {
                g_charge_maintenance_build_timeout_count++;
            }
            *p_fault = SVC_CHARGE_FAULT_INPUT_VOLTAGE;
            return PLATFORM_ERR_HW;
        }
    }
    voltage_target_mv = p_config->maintenance.voltage_target_mv;
    status = svc_charge_control_calculate_cv(p_value, current_duty_permille,
                                             voltage_target_mv, p_result);
    if(PLATFORM_ERR_OK != status)
    {
        *p_fault = SVC_CHARGE_FAULT_DUTY_LIMIT;
        return status;
    }
    /* Low Buck output plus battery discharge is not proof of missing input.
     * CV may intentionally reduce output above/inside its voltage deadband.
     * Stop power transfer normally in that case; retain a fault when voltage
     * regulation actually needs more supply. BUILD has its bounded check. */
    if(SVC_CHARGE_MAINTENANCE_PHASE_BUILD != g_charge_maintenance_phase)
    {
        *p_fault = svc_charge_protection_update_maintenance_reverse(p_value);
        if(SVC_CHARGE_FAULT_NONE != *p_fault)
        {
            if(SVC_CHARGE_FAULT_INPUT_VOLTAGE == *p_fault &&
               (int32_t)p_config->voltage_deadband_mv >=
                   p_result->voltage_error_mv)
            {
                if(UINT32_MAX != g_charge_maintenance_low_output_wait_count)
                {
                    g_charge_maintenance_low_output_wait_count++;
                }
                *p_fault = SVC_CHARGE_FAULT_NONE;
                return PLATFORM_ERR_BUSY;
            }
            return PLATFORM_ERR_HW;
        }
    }
    if(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase)
    {
        /* In the voltage deadband the ordinary CV holds D even when no output
         * has been established. Probe upward only at/below the target and
         * below the current limit; never override a CV/current down-step. */
        if(0U == p_result->duty_changed &&
           p_value->battery_voltage_mv <= voltage_target_mv &&
           SVC_CHARGE_MAINTENANCE_SUPPLY_MIN_MA > p_value->input_current_ma &&
           (int32_t)p_config->current_deadband_ma < p_result->current_error_ma)
        {
            if(BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE -
                   current_duty_permille >=
               p_config->control_step_permille)
            {
                p_result->duty_permille = current_duty_permille +
                                          p_config->control_step_permille;
                p_result->duty_changed  = 1U;
            }
            else
            {
                p_result->duty_high_limited = 1U;
            }
        }
        if(p_result->duty_permille > current_duty_permille)
        {
            if(g_maintenance_build_step_ticks < p_config->start_settle_ticks)
            {
                p_result->duty_permille = current_duty_permille;
                p_result->duty_changed  = 0U;
            }
            else
            {
                g_maintenance_build_step_ticks = 0U;
            }
        }
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
