/**
 ******************************************************************************
 *@file               :   svc_charge_mppt.c
 *@brief              :   Verify power perturbation using direct duty steps.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_mppt.h"
#include "svc_charge_config.h"
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/
#define SVC_CHARGE_MPPT_DUTY_UP_STEP_PERMILLE (1U)
#define SVC_CHARGE_MPPT_DUTY_DOWN_STEP_PERMILLE (1U)
#define SVC_CHARGE_MPPT_SETTLE_TICKS (300U / SVC_CHARGE_PROCESS_PERIOD_MS)
#define SVC_CHARGE_MPPT_MEASURE_TICKS (200U / SVC_CHARGE_PROCESS_PERIOD_MS)
#define SVC_CHARGE_MPPT_POWER_DEADBAND_UW (100000U)
#define SVC_CHARGE_MPPT_TREND_MAX_COMPARISONS (8U)
/* Trial startup search: retain the validated software measurement cadence.
 * These step/timeout values still require board verification. */
#define SVC_CHARGE_MPPT_STARTUP_STEP_PERMILLE (5U)
#define SVC_CHARGE_MPPT_STARTUP_MAX_WINDOWS (20U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
uint32_t                g_charge_mppt_power_raw;
uint32_t                g_charge_mppt_previous_power_raw;
uint64_t                g_charge_mppt_power_sum_raw;
uint32_t                g_charge_mppt_sample_count;
uint8_t                 g_charge_mppt_direction_increase;
uint8_t                 g_charge_mppt_previous_power_valid;
uint8_t                 g_charge_mppt_last_direction_increase;
int8_t                  g_charge_mppt_power_change;
uint32_t                g_charge_mppt_compare_count;
svc_charge_mppt_phase_t g_charge_mppt_phase;
static uint32_t         g_charge_mppt_trend_reference_power_uw;
static uint8_t          g_charge_mppt_trend_compare_count;
static uint16_t         g_charge_mppt_expected_duty_permille;
static uint16_t         g_charge_mppt_reference_duty_permille;
static uint32_t         g_charge_mppt_settle_tick_count;
static uint8_t          g_charge_mppt_duty_known;
static uint8_t          g_charge_mppt_perturb_pending;
static uint8_t          g_charge_mppt_startup_active;
static uint8_t          g_charge_mppt_startup_window_count;
static uint8_t          g_charge_mppt_reacquire_trials;
/* private  functions  ------------------------------------------------------*/
static void restart_window(void)
{
    g_charge_mppt_settle_tick_count = 0U;
    g_charge_mppt_sample_count      = 0U;
    g_charge_mppt_power_sum_raw     = 0U;
}

static void invalidate_comparison(uint16_t duty_permille)
{
    g_charge_mppt_previous_power_valid     = 0U;
    g_charge_mppt_perturb_pending          = 0U;
    g_charge_mppt_trend_reference_power_uw = 0U;
    g_charge_mppt_trend_compare_count      = 0U;
    g_charge_mppt_expected_duty_permille   = duty_permille;
    g_charge_mppt_duty_known               = 1U;
    restart_window();
}

static void perturb_duty(uint16_t                  current_duty_permille,
                         svc_charge_mppt_result_t *p_result)
{
    uint8_t  direction_before = g_charge_mppt_direction_increase;
    uint16_t up_step          = (0U != g_charge_mppt_startup_active)
                                    ? SVC_CHARGE_MPPT_STARTUP_STEP_PERMILLE
                                    : SVC_CHARGE_MPPT_DUTY_UP_STEP_PERMILLE;

    /* Reverse at a boundary before stepping. Never compare an unchanged D. */
    if(BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE == current_duty_permille)
    {
        g_charge_mppt_direction_increase = 0U;
    }
    else if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE == current_duty_permille)
    {
        g_charge_mppt_direction_increase = 1U;
    }
    if(direction_before != g_charge_mppt_direction_increase)
    {
        g_charge_mppt_trend_reference_power_uw = g_charge_mppt_power_raw;
        g_charge_mppt_trend_compare_count      = 0U;
    }
    if(0U != g_charge_mppt_direction_increase)
    {
        if((BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE - current_duty_permille) <
           up_step)
        {
            p_result->duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE;
        }
        else
        {
            p_result->duty_permille = current_duty_permille + up_step;
        }
    }
    else
    {
        if((current_duty_permille - BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE) <
           SVC_CHARGE_MPPT_DUTY_DOWN_STEP_PERMILLE)
        {
            p_result->duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE;
        }
        else
        {
            p_result->duty_permille = current_duty_permille -
                                      SVC_CHARGE_MPPT_DUTY_DOWN_STEP_PERMILLE;
        }
    }
    p_result->duty_changed                = 1U;
    g_charge_mppt_reference_duty_permille = current_duty_permille;
    g_charge_mppt_expected_duty_permille  = p_result->duty_permille;
    g_charge_mppt_perturb_pending         = 1U;
    restart_window();
}

/* exported functions -------------------------------------------------------*/
void svc_charge_mppt_reset(void)
{
    g_charge_mppt_phase                    = SVC_CHARGE_MPPT_PHASE_OFF;
    g_charge_mppt_startup_active           = 0U;
    g_charge_mppt_startup_window_count     = 0U;
    g_charge_mppt_reacquire_trials         = 0U;
    g_charge_mppt_power_raw                = 0U;
    g_charge_mppt_previous_power_raw       = 0U;
    g_charge_mppt_previous_power_valid     = 0U;
    g_charge_mppt_direction_increase       = 1U;
    g_charge_mppt_duty_known               = 0U;
    g_charge_mppt_perturb_pending          = 0U;
    g_charge_mppt_last_direction_increase  = 0U;
    g_charge_mppt_power_change             = 0;
    g_charge_mppt_compare_count            = 0U;
    g_charge_mppt_trend_reference_power_uw = 0U;
    g_charge_mppt_trend_compare_count      = 0U;
    restart_window();
}

void svc_charge_mppt_startup_begin(void)
{
    svc_charge_mppt_reset();
    g_charge_mppt_startup_active = 1U;
    g_charge_mppt_phase          = SVC_CHARGE_MPPT_PHASE_STARTUP;
}

svc_charge_mppt_phase_t svc_charge_mppt_get_phase(void)
{
    return g_charge_mppt_phase;
}

void svc_charge_mppt_pause(uint16_t current_duty_permille)
{
    g_charge_mppt_reacquire_trials = 0U;
    g_charge_mppt_phase            = SVC_CHARGE_MPPT_PHASE_PAUSED;
    g_charge_mppt_startup_active   = 0U;
    invalidate_comparison(current_duty_permille);
}

void svc_charge_mppt_resume(uint16_t current_duty_permille)
{
    g_charge_mppt_startup_active = 0U;
    /* The controller moved the operating point. UP is only a fresh probe,
     * never a retained conclusion; perturb_duty reverses at the upper bound. */
    g_charge_mppt_direction_increase      = 1U;
    g_charge_mppt_last_direction_increase = 0U;
    g_charge_mppt_power_change            = 0;
    g_charge_mppt_reacquire_trials        = 0U;
    invalidate_comparison(current_duty_permille);
    g_charge_mppt_phase = SVC_CHARGE_MPPT_PHASE_REACQUIRE;
}

platform_err_t svc_charge_mppt_update(const svc_measure_value_t *p_value,
                                      uint16_t current_duty_permille,
                                      svc_charge_mppt_result_t *p_result)
{
    uint64_t average_power_uw;
    uint8_t  comparison_ready = 0U;

    if(NULL == p_value || NULL == p_result ||
       0U == p_value->battery_voltage_mv ||
       BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > current_duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < current_duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    p_result->duty_permille = current_duty_permille;
    if(0U == g_charge_mppt_startup_active &&
       SVC_CHARGE_MPPT_PHASE_REACQUIRE != g_charge_mppt_phase)
    {
        g_charge_mppt_phase = SVC_CHARGE_MPPT_PHASE_TRACKING;
    }
    p_result->duty_changed      = 0U;
    p_result->duty_high_limited = (BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE ==
                                   current_duty_permille)
                                      ? 1U
                                      : 0U;
    if(0U == g_charge_mppt_duty_known ||
       g_charge_mppt_expected_duty_permille != current_duty_permille)
    {
        if(0U != g_charge_mppt_duty_known)
        {
            /* An external controller changed D: do not resume coarse steps. */
            g_charge_mppt_startup_active = 0U;
            g_charge_mppt_phase          = SVC_CHARGE_MPPT_PHASE_TRACKING;
        }
        invalidate_comparison(current_duty_permille);
    }
    if(g_charge_mppt_settle_tick_count < SVC_CHARGE_MPPT_SETTLE_TICKS)
    {
        g_charge_mppt_settle_tick_count++;
        return PLATFORM_ERR_OK;
    }

    g_charge_mppt_power_sum_raw += (uint64_t)p_value->battery_voltage_mv *
                                   p_value->input_current_ma;
    g_charge_mppt_sample_count++;
    if(g_charge_mppt_sample_count < SVC_CHARGE_MPPT_MEASURE_TICKS)
    {
        return PLATFORM_ERR_OK;
    }
    average_power_uw = g_charge_mppt_power_sum_raw / g_charge_mppt_sample_count;
    if(UINT32_MAX < average_power_uw)
    {
        invalidate_comparison(current_duty_permille);
        return PLATFORM_ERR_PARAM;
    }
    g_charge_mppt_power_raw = (uint32_t)average_power_uw;
    if(0U != g_charge_mppt_previous_power_valid &&
       0U != g_charge_mppt_perturb_pending)
    {
        g_charge_mppt_last_direction_increase =
            (current_duty_permille > g_charge_mppt_reference_duty_permille)
                ? 1U
                : 0U;
        g_charge_mppt_direction_increase =
            g_charge_mppt_last_direction_increase;
        g_charge_mppt_power_change = 0;
        g_charge_mppt_trend_compare_count++;
        if(g_charge_mppt_power_raw > g_charge_mppt_previous_power_raw &&
           (g_charge_mppt_power_raw - g_charge_mppt_previous_power_raw) >
               SVC_CHARGE_MPPT_POWER_DEADBAND_UW)
        {
            g_charge_mppt_power_change = 1;
        }
        else if(g_charge_mppt_previous_power_raw > g_charge_mppt_power_raw &&
                (g_charge_mppt_previous_power_raw - g_charge_mppt_power_raw) >
                    SVC_CHARGE_MPPT_POWER_DEADBAND_UW)
        {
            g_charge_mppt_power_change = -1;
            g_charge_mppt_direction_increase =
                (0U == g_charge_mppt_last_direction_increase) ? 1U : 0U;
        }
        /* Do not discard a cumulative loss as successive sub-deadband steps.
         * This bounded reference is only a trend guard, not a global MPP. */
        if(0 <= g_charge_mppt_power_change &&
           g_charge_mppt_trend_reference_power_uw > g_charge_mppt_power_raw &&
           (g_charge_mppt_trend_reference_power_uw - g_charge_mppt_power_raw) >
               SVC_CHARGE_MPPT_POWER_DEADBAND_UW)
        {
            g_charge_mppt_power_change = -1;
            g_charge_mppt_direction_increase =
                (0U == g_charge_mppt_last_direction_increase) ? 1U : 0U;
        }
        g_charge_mppt_compare_count++;
        comparison_ready = 1U;
    }
    if(0U != g_charge_mppt_startup_active)
    {
        g_charge_mppt_startup_window_count++;
        if(0U != comparison_ready && 0 > g_charge_mppt_power_change)
        {
            /* Undo the last coarse uphill trial, then establish a new fine
             * baseline. Never compare power measured at the rejected D. */
            p_result->duty_permille = g_charge_mppt_reference_duty_permille;
            p_result->duty_changed  = 1U;
            g_charge_mppt_startup_active     = 0U;
            g_charge_mppt_phase              = SVC_CHARGE_MPPT_PHASE_TRACKING;
            g_charge_mppt_direction_increase = 1U;
            invalidate_comparison(p_result->duty_permille);
            return PLATFORM_ERR_OK;
        }
        if(SVC_CHARGE_MPPT_STARTUP_MAX_WINDOWS <=
               g_charge_mppt_startup_window_count ||
           BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE == current_duty_permille)
        {
            g_charge_mppt_startup_active = 0U;
            g_charge_mppt_phase          = SVC_CHARGE_MPPT_PHASE_TRACKING;
        }
    }
    if(SVC_CHARGE_MPPT_PHASE_REACQUIRE == g_charge_mppt_phase &&
       0U != comparison_ready)
    {
        g_charge_mppt_reacquire_trials++;
        if(0 < g_charge_mppt_power_change)
        {
            /* Only a measured gain establishes the new tracking direction. */
            g_charge_mppt_phase = SVC_CHARGE_MPPT_PHASE_TRACKING;
        }
        else
        {
            /* Reject a losing or inconclusive neighbor. Measure the restored
             * point again before probing the other side; at most two trials. */
            p_result->duty_permille = g_charge_mppt_reference_duty_permille;
            p_result->duty_changed  = 1U;
            g_charge_mppt_direction_increase =
                (0U == g_charge_mppt_last_direction_increase) ? 1U : 0U;
            if(2U <= g_charge_mppt_reacquire_trials ||
               BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE ==
                   p_result->duty_permille ||
               BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE ==
                   p_result->duty_permille)
            {
                g_charge_mppt_phase = SVC_CHARGE_MPPT_PHASE_TRACKING;
            }
            invalidate_comparison(p_result->duty_permille);
            return PLATFORM_ERR_OK;
        }
    }
    g_charge_mppt_previous_power_raw   = g_charge_mppt_power_raw;
    g_charge_mppt_previous_power_valid = 1U;
    if(0U == comparison_ready ||
       g_charge_mppt_direction_increase !=
           g_charge_mppt_last_direction_increase ||
       g_charge_mppt_trend_compare_count >=
           SVC_CHARGE_MPPT_TREND_MAX_COMPARISONS)
    {
        g_charge_mppt_trend_reference_power_uw = g_charge_mppt_power_raw;
        g_charge_mppt_trend_compare_count      = 0U;
    }
    else if(g_charge_mppt_power_raw > g_charge_mppt_trend_reference_power_uw)
    {
        g_charge_mppt_trend_reference_power_uw = g_charge_mppt_power_raw;
    }
    perturb_duty(current_duty_permille, p_result);
    return PLATFORM_ERR_OK;
}

/* end of file --------------------------------------------------------------*/
