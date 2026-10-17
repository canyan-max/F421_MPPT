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
uint16_t                         g_charge_mppt_duty_before_permille;
uint16_t                         g_charge_mppt_duty_after_permille;
uint32_t                         g_charge_mppt_power_before_uw;
uint32_t                         g_charge_mppt_power_after_uw;
uint8_t                          g_charge_mppt_last_direction_increase;
int8_t                           g_charge_mppt_power_change;
uint32_t                         g_charge_mppt_compare_count;
static uint16_t                  g_charge_mppt_expected_duty_permille;
static uint16_t                  g_charge_mppt_reference_duty_permille;
static uint32_t                  g_charge_mppt_settle_tick_count;
static uint8_t                   g_charge_mppt_duty_known;
static uint8_t                   g_charge_mppt_perturb_pending;
volatile svc_charge_mppt_trace_t g_charge_mppt_trace[SVC_CHARGE_MPPT_TRACE_NUM];
volatile uint16_t                g_charge_mppt_trace_write_index;
volatile uint16_t                g_charge_mppt_trace_count;
volatile svc_charge_mppt_trace_freeze_t g_charge_mppt_trace_freeze;
static uint32_t                         g_charge_mppt_trace_sequence;
static uint32_t                         g_charge_mppt_trace_service_tick;
static uint32_t                         g_charge_mppt_trace_measure_sequence;

/* private  functions  ------------------------------------------------------*/
static void save_comparison(const svc_measure_value_t      *p_value,
                            const svc_charge_mppt_result_t *p_result)
{
    svc_charge_mppt_trace_t snapshot = {0};
    uint16_t                index    = g_charge_mppt_trace_write_index;

    if(NULL == p_value || NULL == p_result ||
       SVC_CHARGE_MPPT_TRACE_RUNNING != g_charge_mppt_trace_freeze ||
       index >= sizeof(g_charge_mppt_trace) / sizeof(g_charge_mppt_trace[0U]))
    {
        return;
    }
    g_charge_mppt_trace_sequence++;
    snapshot.sequence               = g_charge_mppt_trace_sequence;
    snapshot.service_tick           = g_charge_mppt_trace_service_tick;
    snapshot.measure_sequence       = g_charge_mppt_trace_measure_sequence;
    snapshot.compare_count          = g_charge_mppt_compare_count;
    snapshot.power_before_uw        = g_charge_mppt_power_before_uw;
    snapshot.power_after_uw         = g_charge_mppt_power_after_uw;
    snapshot.input_voltage_end_mv   = p_value->input_voltage_mv;
    snapshot.battery_voltage_end_mv = p_value->battery_voltage_mv;
    snapshot.input_current_end_ma   = p_value->input_current_ma;
    snapshot.duty_before_permille   = g_charge_mppt_duty_before_permille;
    snapshot.duty_after_permille    = g_charge_mppt_duty_after_permille;
    snapshot.next_duty_requested_permille = p_result->duty_permille;
    snapshot.perturb_increase        = g_charge_mppt_last_direction_increase;
    snapshot.next_direction_increase = g_charge_mppt_direction_increase;
    snapshot.power_change            = g_charge_mppt_power_change;
    g_charge_mppt_trace[index].valid = 0U;
    g_charge_mppt_trace[index]       = snapshot;
    /* Publish last. Read the ring after freeze for a stable complete history.
     */
    g_charge_mppt_trace[index].valid = 1U;
    index++;
    if(index >= sizeof(g_charge_mppt_trace) / sizeof(g_charge_mppt_trace[0U]))
    {
        index = 0U;
    }
    g_charge_mppt_trace_write_index = index;
    if(g_charge_mppt_trace_count <
       sizeof(g_charge_mppt_trace) / sizeof(g_charge_mppt_trace[0U]))
    {
        g_charge_mppt_trace_count++;
    }
}

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
void svc_charge_mppt_trace_clear(void)
{
    svc_charge_mppt_trace_t empty_snapshot = {0};
    uint32_t                index;

    for(index = 0U;
        index < sizeof(g_charge_mppt_trace) / sizeof(g_charge_mppt_trace[0U]);
        index++)
    {
        g_charge_mppt_trace[index] = empty_snapshot;
    }
    g_charge_mppt_trace_write_index      = 0U;
    g_charge_mppt_trace_count            = 0U;
    g_charge_mppt_trace_sequence         = 0U;
    g_charge_mppt_trace_service_tick     = 0U;
    g_charge_mppt_trace_measure_sequence = 0U;
    g_charge_mppt_trace_freeze           = SVC_CHARGE_MPPT_TRACE_RUNNING;
}

void svc_charge_mppt_trace_freeze(svc_charge_mppt_trace_freeze_t reason)
{
    /* Do not let a startup fault with no comparisons disable later capture. */
    if(SVC_CHARGE_MPPT_TRACE_CV > reason ||
       SVC_CHARGE_MPPT_TRACE_STOP < reason || 0U == g_charge_mppt_trace_count ||
       SVC_CHARGE_MPPT_TRACE_RUNNING != g_charge_mppt_trace_freeze)
    {
        return;
    }
    g_charge_mppt_trace_freeze = reason;
}

void svc_charge_mppt_trace_set_context(uint32_t service_tick,
                                       uint32_t measure_sequence)
{
    g_charge_mppt_trace_service_tick     = service_tick;
    g_charge_mppt_trace_measure_sequence = measure_sequence;
}

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

uint32_t svc_charge_mppt_get_compare_count(void)
{
    return g_charge_mppt_compare_count;
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
        comparison_ready = 1U;
    }
    g_charge_mppt_previous_power_raw   = g_charge_mppt_power_raw;
    g_charge_mppt_previous_power_valid = 1U;
    perturb_duty(current_duty_permille, p_result);
    if(0U != comparison_ready)
    {
        save_comparison(p_value, p_result);
    }
    return PLATFORM_ERR_OK;
}

/* end of file --------------------------------------------------------------*/
