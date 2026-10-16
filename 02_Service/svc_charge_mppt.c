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
#define SVC_CHARGE_MPPT_DUTY_STEP_PERMILLE (1U)
#define SVC_CHARGE_MPPT_SETTLE_TICKS (300U / SVC_CHARGE_PROCESS_PERIOD_MS)
#define SVC_CHARGE_MPPT_MEASURE_TICKS (200U / SVC_CHARGE_PROCESS_PERIOD_MS)
#define SVC_CHARGE_MPPT_POWER_DEADBAND_UW (100000U)

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
uint32_t g_charge_mppt_power_raw;
uint32_t g_charge_mppt_previous_power_raw;
uint64_t g_charge_mppt_power_sum_raw;
uint32_t g_charge_mppt_sample_count;
uint8_t  g_charge_mppt_direction_increase;
uint8_t  g_charge_mppt_previous_power_valid;
/* Completed comparison snapshot; retained until the next valid comparison. */
uint16_t        g_charge_mppt_duty_before_permille;
uint16_t        g_charge_mppt_duty_after_permille;
uint32_t        g_charge_mppt_power_before_uw;
uint32_t        g_charge_mppt_power_after_uw;
uint8_t         g_charge_mppt_last_direction_increase;
int8_t          g_charge_mppt_power_change;
uint32_t        g_charge_mppt_compare_count;
static uint16_t g_charge_mppt_expected_duty_permille;
static uint16_t g_charge_mppt_reference_duty_permille;
static uint32_t g_charge_mppt_settle_tick_count;
static uint8_t  g_charge_mppt_duty_known;
static uint8_t  g_charge_mppt_perturb_pending;

/* private  functions  ------------------------------------------------------*/
static void restart_window(void)
{
    g_charge_mppt_settle_tick_count = 0U;
    g_charge_mppt_sample_count      = 0U;
    g_charge_mppt_power_sum_raw     = 0U;
}

static void invalidate_comparison(uint16_t duty_permille)
{
    g_charge_mppt_previous_power_valid   = 0U;
    g_charge_mppt_perturb_pending        = 0U;
    g_charge_mppt_expected_duty_permille = duty_permille;
    g_charge_mppt_duty_known             = 1U;
    restart_window();
}

static void perturb_duty(uint16_t                  current_duty_permille,
                         svc_charge_mppt_result_t *p_result)
{
    /* Reverse at a boundary before stepping. Never compare an unchanged D. */
    if(BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE == current_duty_permille)
    {
        g_charge_mppt_direction_increase = 0U;
    }
    else if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE == current_duty_permille)
    {
        g_charge_mppt_direction_increase = 1U;
    }
    if(0U != g_charge_mppt_direction_increase)
    {
        p_result->duty_permille = current_duty_permille +
                                  SVC_CHARGE_MPPT_DUTY_STEP_PERMILLE;
    }
    else
    {
        p_result->duty_permille = current_duty_permille -
                                  SVC_CHARGE_MPPT_DUTY_STEP_PERMILLE;
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
    g_charge_mppt_power_raw               = 0U;
    g_charge_mppt_previous_power_raw      = 0U;
    g_charge_mppt_previous_power_valid    = 0U;
    g_charge_mppt_direction_increase      = 1U;
    g_charge_mppt_duty_known              = 0U;
    g_charge_mppt_perturb_pending         = 0U;
    g_charge_mppt_duty_before_permille    = 0U;
    g_charge_mppt_duty_after_permille     = 0U;
    g_charge_mppt_power_before_uw         = 0U;
    g_charge_mppt_power_after_uw          = 0U;
    g_charge_mppt_last_direction_increase = 0U;
    g_charge_mppt_power_change            = 0;
    g_charge_mppt_compare_count           = 0U;
    restart_window();
}

void svc_charge_mppt_pause(uint16_t current_duty_permille)
{
    invalidate_comparison(current_duty_permille);
}

platform_err_t svc_charge_mppt_update(const svc_measure_value_t *p_value,
                                      uint16_t current_duty_permille,
                                      svc_charge_mppt_result_t *p_result)
{
    uint64_t average_power_uw;

    if(NULL == p_value || NULL == p_result ||
       0U == p_value->battery_voltage_mv ||
       BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > current_duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < current_duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    p_result->duty_permille     = current_duty_permille;
    p_result->duty_changed      = 0U;
    p_result->duty_high_limited = (BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE ==
                                   current_duty_permille)
                                      ? 1U
                                      : 0U;
    if(0U == g_charge_mppt_duty_known ||
       g_charge_mppt_expected_duty_permille != current_duty_permille)
    {
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
        return PLATFORM_ERR_PARAM;
    }
    g_charge_mppt_power_raw = (uint32_t)average_power_uw;
    if(0U != g_charge_mppt_previous_power_valid &&
       0U != g_charge_mppt_perturb_pending)
    {
        g_charge_mppt_duty_before_permille =
            g_charge_mppt_reference_duty_permille;
        g_charge_mppt_duty_after_permille = current_duty_permille;
        g_charge_mppt_power_before_uw     = g_charge_mppt_previous_power_raw;
        g_charge_mppt_power_after_uw      = g_charge_mppt_power_raw;
        g_charge_mppt_last_direction_increase =
            (current_duty_permille > g_charge_mppt_reference_duty_permille)
                ? 1U
                : 0U;
        g_charge_mppt_direction_increase =
            g_charge_mppt_last_direction_increase;
        g_charge_mppt_power_change = 0;
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
        /* Inside the noise band retain direction, without declaring a gain. */
        g_charge_mppt_compare_count++;
    }
    g_charge_mppt_previous_power_raw   = g_charge_mppt_power_raw;
    g_charge_mppt_previous_power_valid = 1U;
    perturb_duty(current_duty_permille, p_result);
    return PLATFORM_ERR_OK;
}

/* end of file --------------------------------------------------------------*/
