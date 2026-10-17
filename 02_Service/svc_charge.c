/**
 ******************************************************************************
 *@file               :   svc_charge.c
 *@brief              :   Start the Buck power path after validated sampling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <string.h>
#include "svc_charge.h"
#include "svc_charge_control.h"
#include "svc_charge_maintenance.h"
#include "svc_charge_mppt.h"
#include "svc_measure.h"
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/
#define SVC_CHARGE_CC_RELEASE_MARGIN_MA (100U)
#define SVC_CHARGE_TRACE_NEAR_DUTY_PERMILLE (930U)

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_TRACE_SLOT_T
{
    SVC_CHARGE_TRACE_NEAR = 0U,
    SVC_CHARGE_TRACE_TOP,
    SVC_CHARGE_TRACE_AFTER_500MS,
    SVC_CHARGE_TRACE_AFTER_2S,
    SVC_CHARGE_TRACE_AFTER_10S,
    SVC_CHARGE_TRACE_MIN_VIN,
    SVC_CHARGE_TRACE_STATE_CHANGE,
    SVC_CHARGE_TRACE_LEAVE_HIGH,
    SVC_CHARGE_TRACE_FAULT,
    SVC_CHARGE_TRACE_LATEST,
    SVC_CHARGE_TRACE_NUM,
} svc_charge_trace_slot_t;

typedef enum SVC_CHARGE_TRACE_SOURCE_T
{
    SVC_CHARGE_TRACE_SOURCE_NONE = 0U,
    SVC_CHARGE_TRACE_SOURCE_MPPT,
    SVC_CHARGE_TRACE_SOURCE_CC,
    SVC_CHARGE_TRACE_SOURCE_CV,
    SVC_CHARGE_TRACE_SOURCE_MAINTENANCE,
} svc_charge_trace_source_t;

typedef struct SVC_CHARGE_TRACE_SNAPSHOT_T
{
    uint32_t service_tick; /* Relative 10 ms ticks; not ADC frames. */
    uint32_t measure_sequence;
    uint32_t mppt_compare_before;
    uint32_t mppt_compare_after;
    svc_measure_value_t       measure;
    uint16_t                  duty_before;
    uint16_t                  duty_after;
    uint16_t                  cv_duty_ceiling_permille;
    svc_charge_state_t        state_before;
    svc_charge_state_t        state_after;
    uint8_t                   cc_before;
    uint8_t                   cc_after;
    svc_charge_trace_source_t control_source;
    svc_charge_fault_t        fault;
    uint8_t                   duty_high_limited;
    uint8_t                   cv_duty_ceiling_limited;
    uint8_t                   measurement_valid;
    uint8_t                   valid;
} svc_charge_trace_snapshot_t;

/* variables ----------------------------------------------------------------*/
svc_charge_state_t  g_charge_state = SVC_CHARGE_STATE_OFF;
svc_charge_fault_t  g_charge_fault;
platform_err_t      g_charge_status;
uint16_t            g_charge_duty_permille;
uint32_t            g_charge_last_sequence;
uint16_t            g_charge_stale_tick_count;
uint16_t            g_charge_settle_tick_count;
int32_t             g_charge_current_error_ma;
int32_t             g_charge_voltage_error_mv;
uint32_t            g_charge_recovery_tick_count;
uint8_t             g_charge_recovery_allowed;
uint8_t             g_charge_recovery_armed;
svc_measure_value_t g_charge_fault_measure_snapshot;
svc_charge_state_t  g_charge_fault_state_snapshot;
uint16_t            g_charge_fault_duty_snapshot;
uint32_t            g_charge_fault_sequence_snapshot;
uint8_t             g_charge_fault_snapshot_valid;
uint8_t             g_charge_duty_high_limited;
/* Within CONSTANT_CURRENT: zero selects MPPT, nonzero selects CC. */
uint8_t g_charge_current_limited;
/* Trial: normal CV may recover duty only up to its entry operating point. */
uint16_t g_charge_cv_duty_ceiling_permille;
uint8_t  g_charge_cv_duty_ceiling_active;
uint8_t  g_charge_cv_duty_ceiling_limited;
/* Fixed slots latch once except MIN_VIN and LATEST. Watch-only diagnostics. */
volatile svc_charge_trace_snapshot_t g_charge_trace[SVC_CHARGE_TRACE_NUM];
volatile uint8_t                     g_charge_trace_clear_request;
uint8_t                              g_charge_trace_armed;
static uint32_t                      g_charge_trace_service_tick;
static uint32_t                      g_charge_trace_top_tick;
static svc_measure_value_t           g_charge_trace_measure;
static uint32_t                      g_charge_trace_measure_sequence;
static uint8_t                       g_charge_trace_measure_valid;
static svc_charge_trace_source_t     g_charge_trace_source;

/* private  functions  ------------------------------------------------------*/
static void reset_trace(void)
{
    uint32_t                    index;
    svc_charge_trace_snapshot_t empty_snapshot;

    memset(&empty_snapshot, 0, sizeof(empty_snapshot));
    for(index = 0U; index < sizeof(g_charge_trace) / sizeof(g_charge_trace[0U]);
        index++)
    {
        g_charge_trace[index] = empty_snapshot;
    }
    g_charge_trace_armed         = 0U;
    g_charge_trace_service_tick  = 0U;
    g_charge_trace_top_tick      = 0U;
    g_charge_trace_clear_request = 0U;
    svc_charge_mppt_trace_clear();
}

static void save_trace(svc_charge_trace_slot_t            slot,
                       const svc_charge_trace_snapshot_t *p_snapshot)
{
    if(NULL == p_snapshot || SVC_CHARGE_TRACE_NUM <= slot)
    {
        return;
    }
    g_charge_trace[slot].valid = 0U;
    g_charge_trace[slot]       = *p_snapshot;
    /* Publish last. Inspect fixed slots only after valid becomes one. */
    g_charge_trace[slot].valid = 1U;
}

static void update_trace(svc_charge_trace_snapshot_t *p_snapshot)
{
    uint32_t              elapsed_ticks;
    uint32_t              index;
    static const uint32_t g_trace_delays_ms[] = {500U, 2000U, 10000U};

    if(NULL == p_snapshot)
    {
        return;
    }
    p_snapshot->duty_after               = g_charge_duty_permille;
    p_snapshot->state_after              = g_charge_state;
    p_snapshot->cc_after                 = g_charge_current_limited;
    p_snapshot->control_source           = g_charge_trace_source;
    p_snapshot->fault                    = g_charge_fault;
    p_snapshot->duty_high_limited        = g_charge_duty_high_limited;
    p_snapshot->cv_duty_ceiling_permille = g_charge_cv_duty_ceiling_permille;
    p_snapshot->cv_duty_ceiling_limited  = g_charge_cv_duty_ceiling_limited;
    p_snapshot->mppt_compare_after       = svc_charge_mppt_get_compare_count();
    p_snapshot->measurement_valid        = g_charge_trace_measure_valid;
    p_snapshot->measure_sequence         = g_charge_trace_measure_sequence;
    if(0U != g_charge_trace_measure_valid)
    {
        p_snapshot->measure = g_charge_trace_measure;
    }
    /* Fault evidence must survive even if the near-duty trigger never fired. */
    if(0U == g_charge_trace[SVC_CHARGE_TRACE_FAULT].valid &&
       SVC_CHARGE_STATE_FAULT == g_charge_state)
    {
        save_trace(SVC_CHARGE_TRACE_FAULT, p_snapshot);
    }
    /* Normal CV can now stay below 930; retain its entry without a high-D
     * trigger so the trial ceiling is still observable in a fixed snapshot. */
    if(0U == g_charge_trace[SVC_CHARGE_TRACE_STATE_CHANGE].valid &&
       p_snapshot->state_before != p_snapshot->state_after &&
       (0U != g_charge_trace_armed ||
        SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state))
    {
        save_trace(SVC_CHARGE_TRACE_STATE_CHANGE, p_snapshot);
    }
    if(0U == g_charge_trace_armed)
    {
        if(SVC_CHARGE_TRACE_NEAR_DUTY_PERMILLE > p_snapshot->duty_before &&
           SVC_CHARGE_TRACE_NEAR_DUTY_PERMILLE > p_snapshot->duty_after)
        {
            return;
        }
        g_charge_trace_armed = 1U;
        save_trace(SVC_CHARGE_TRACE_NEAR, p_snapshot);
    }
    if(0U == g_charge_trace[SVC_CHARGE_TRACE_TOP].valid &&
       (BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE == p_snapshot->duty_before ||
        BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE == p_snapshot->duty_after))
    {
        g_charge_trace_top_tick = g_charge_trace_service_tick;
        save_trace(SVC_CHARGE_TRACE_TOP, p_snapshot);
    }
    if(0U != g_charge_trace[SVC_CHARGE_TRACE_TOP].valid)
    {
        elapsed_ticks = g_charge_trace_service_tick - g_charge_trace_top_tick;
        for(index = 0U;
            index < sizeof(g_trace_delays_ms) / sizeof(g_trace_delays_ms[0U]);
            index++)
        {
            svc_charge_trace_slot_t
                slot = (svc_charge_trace_slot_t)(SVC_CHARGE_TRACE_AFTER_500MS +
                                                 index);
            if(0U == g_charge_trace[slot].valid &&
               elapsed_ticks >=
                   g_trace_delays_ms[index] / SVC_CHARGE_PROCESS_PERIOD_MS)
            {
                save_trace(slot, p_snapshot);
            }
        }
    }
    if(0U != p_snapshot->measurement_valid &&
       (0U == g_charge_trace[SVC_CHARGE_TRACE_MIN_VIN].valid ||
        0U == g_charge_trace[SVC_CHARGE_TRACE_MIN_VIN].measurement_valid ||
        p_snapshot->measure.input_voltage_mv <
            g_charge_trace[SVC_CHARGE_TRACE_MIN_VIN].measure.input_voltage_mv))
    {
        save_trace(SVC_CHARGE_TRACE_MIN_VIN, p_snapshot);
    }
    if(0U == g_charge_trace[SVC_CHARGE_TRACE_LEAVE_HIGH].valid &&
       SVC_CHARGE_TRACE_NEAR_DUTY_PERMILLE <= p_snapshot->duty_before &&
       SVC_CHARGE_TRACE_NEAR_DUTY_PERMILLE > p_snapshot->duty_after)
    {
        save_trace(SVC_CHARGE_TRACE_LEAVE_HIGH, p_snapshot);
    }
    /* Refresh only every 500 ms; earlier event slots remain latched. */
    if(0U == g_charge_trace[SVC_CHARGE_TRACE_LATEST].valid ||
       (g_charge_trace_service_tick -
        g_charge_trace[SVC_CHARGE_TRACE_LATEST].service_tick) >=
           500U / SVC_CHARGE_PROCESS_PERIOD_MS)
    {
        save_trace(SVC_CHARGE_TRACE_LATEST, p_snapshot);
    }
}
static uint32_t delay_ms_to_ticks(uint32_t delay_ms)
{
    uint32_t ticks = delay_ms / SVC_CHARGE_PROCESS_PERIOD_MS;

    if(0U != delay_ms % SVC_CHARGE_PROCESS_PERIOD_MS)
    {
        ticks++;
    }
    return ticks;
}

static void reset_recovery_runtime(void)
{
    g_charge_recovery_tick_count = 0U;
    g_charge_recovery_allowed    = 0U;
    g_charge_recovery_armed      = 0U;
}

static void reset_mppt_runtime(void)
{
    g_charge_current_limited          = 0U;
    g_charge_cv_duty_ceiling_permille = 0U;
    g_charge_cv_duty_ceiling_active   = 0U;
    g_charge_cv_duty_ceiling_limited  = 0U;
    svc_charge_mppt_reset();
}

static void enter_constant_voltage(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value)
    {
        return;
    }
    svc_charge_maintenance_reset();
    svc_charge_mppt_trace_freeze(SVC_CHARGE_MPPT_TRACE_CV);
    reset_mppt_runtime();
    g_charge_cv_duty_ceiling_permille = g_charge_duty_permille;
    g_charge_cv_duty_ceiling_active   = 1U;
    g_charge_voltage_error_mv         = (int32_t)p_config->voltage_target_mv -
                                        (int32_t)p_value->battery_voltage_mv;
    g_charge_state                    = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
}

static platform_err_t
calculate_running_duty(const svc_measure_value_t   *p_value,
                       uint16_t                     current_duty_permille,
                       svc_charge_control_result_t *p_result)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_charge_mppt_result_t   mppt_result;
    platform_err_t             status;

    if(NULL == p_value || NULL == p_result ||
       0U == p_value->battery_voltage_mv ||
       BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > current_duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < current_duty_permille ||
       p_config->current_target_ma <= p_config->current_deadband_ma)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(0U == g_charge_current_limited)
    {
        if((p_config->current_target_ma - p_config->current_deadband_ma) <=
           p_value->input_current_ma)
        {
            g_charge_current_limited = 1U;
        }
    }
    else if(p_value->input_current_ma < p_config->current_target_ma &&
            (p_config->current_target_ma - p_value->input_current_ma) >=
                SVC_CHARGE_CC_RELEASE_MARGIN_MA &&
            (p_config->current_target_ma - p_value->input_current_ma) >
                p_config->current_deadband_ma)
    {
        g_charge_current_limited = 0U;
    }
    if(0U != g_charge_current_limited)
    {
        g_charge_trace_source = SVC_CHARGE_TRACE_SOURCE_CC;
        svc_charge_mppt_pause(current_duty_permille);
        return svc_charge_control_calculate_cc(p_value, current_duty_permille,
                                               p_config->current_target_ma,
                                               p_result);
    }
    g_charge_trace_source = SVC_CHARGE_TRACE_SOURCE_MPPT;
    svc_charge_mppt_trace_set_context(g_charge_trace_service_tick,
                                      g_charge_trace_measure_sequence);
    status = svc_charge_mppt_update(p_value, current_duty_permille,
                                    &mppt_result);
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    p_result->duty_permille     = mppt_result.duty_permille;
    p_result->duty_changed      = mppt_result.duty_changed;
    p_result->duty_high_limited = mppt_result.duty_high_limited;
    p_result->current_error_ma  = (int32_t)p_config->current_target_ma -
                                  (int32_t)p_value->input_current_ma;
    p_result->voltage_error_mv  = (int32_t)p_config->voltage_target_mv -
                                  (int32_t)p_value->battery_voltage_mv;
    return PLATFORM_ERR_OK;
}

static uint8_t is_start_condition_valid(const svc_measure_value_t *p_value)
{
    uint16_t           duty_permille;
    platform_err_t     status;
    svc_charge_fault_t fault;

    if(NULL == p_value)
    {
        return 0U;
    }
    fault = svc_charge_protection_check_wake(p_value);
    if(SVC_CHARGE_FAULT_NONE != fault ||
       SVC_CHARGE_FAULT_NONE !=
           svc_charge_protection_get_temperature_condition(p_value))
    {
        return 0U;
    }
    status = svc_charge_control_calculate_initial_duty(p_value, &duty_permille);
    if(PLATFORM_ERR_OK == status)
    {
        return (SVC_CHARGE_FAULT_NONE ==
                svc_charge_protection_check_start_voltage(p_value))
                   ? 1U
                   : 0U;
    }
    return (0U != svc_charge_control_is_wake_cv_required(p_value) &&
            0U != svc_charge_control_is_input_voltage_sufficient(p_value, 1U))
               ? 1U
               : 0U;
}

static void capture_fault_snapshot(void)
{
    svc_measure_value_t value;

    g_charge_fault_state_snapshot    = g_charge_state;
    g_charge_fault_duty_snapshot     = g_charge_duty_permille;
    g_charge_fault_sequence_snapshot = svc_measure_get_sequence();
    if(PLATFORM_ERR_OK == svc_measure_get(&value))
    {
        g_charge_fault_measure_snapshot = value;
        g_charge_fault_snapshot_valid   = 1U;
    }
    else
    {
        g_charge_fault_snapshot_valid = 0U;
    }
}

static void enter_fault(svc_charge_fault_t fault, platform_err_t status)
{
    platform_err_t stop_status;

    capture_fault_snapshot();
    stop_status = bsp_power_path_charge_stop();
    svc_charge_mppt_trace_freeze(SVC_CHARGE_MPPT_TRACE_FAULT);
    svc_charge_protection_reset();
    svc_charge_maintenance_reset();
    reset_mppt_runtime();
    g_charge_recovery_tick_count = 0U;
    g_charge_recovery_armed      = (PLATFORM_ERR_OK == stop_status) ? 1U : 0U;
    g_charge_recovery_allowed    = 1U;
    g_charge_fault               = (PLATFORM_ERR_OK == stop_status)
                                       ? fault
                                       : SVC_CHARGE_FAULT_POWER_PATH;
    g_charge_status = (PLATFORM_ERR_OK == stop_status) ? status : stop_status;
    g_charge_state  = SVC_CHARGE_STATE_FAULT;
    if(PLATFORM_ERR_OK == stop_status)
    {
        g_charge_duty_permille = 0U;
    }
}

static platform_err_t get_new_measurement(svc_measure_value_t *p_value,
                                          uint8_t             *p_is_new,
                                          svc_charge_fault_t  *p_fault,
                                          uint8_t allow_missing_battery)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    uint32_t                   sequence;
    platform_err_t             status;
    svc_charge_fault_t         fault;

    if(NULL == p_value || NULL == p_is_new || NULL == p_fault)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_is_new = 0U;
    *p_fault  = SVC_CHARGE_FAULT_NONE;
    status    = svc_measure_get(p_value);
    if(PLATFORM_ERR_OK != status)
    {
        *p_fault = SVC_CHARGE_FAULT_MEASUREMENT;
        return status;
    }
    /* Reuse the exact measurement passed to this control/protection cycle. */
    g_charge_trace_measure          = *p_value;
    g_charge_trace_measure_sequence = svc_measure_get_sequence();
    g_charge_trace_measure_valid    = 1U;
    fault = (0U != allow_missing_battery)
                ? svc_charge_protection_check_wake(p_value)
                : svc_charge_protection_check_immediate(p_value);
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        *p_fault = fault;
        return PLATFORM_ERR_HW;
    }
    if(0U != allow_missing_battery &&
       0U != svc_charge_control_is_wake_cv_required(p_value) &&
       0U == svc_charge_control_is_input_voltage_sufficient(p_value, 1U))
    {
        *p_fault = SVC_CHARGE_FAULT_INPUT_VOLTAGE;
        return PLATFORM_ERR_HW;
    }
    sequence = svc_measure_get_sequence();
    if(sequence != g_charge_last_sequence)
    {
        g_charge_last_sequence    = sequence;
        g_charge_stale_tick_count = 0U;
        *p_is_new                 = 1U;
        fault = svc_charge_protection_update_temperature(p_value);
        if(SVC_CHARGE_FAULT_NONE != fault)
        {
            *p_fault = fault;
            return PLATFORM_ERR_HW;
        }
    }
    else
    {
        if(g_charge_stale_tick_count < p_config->sample_timeout_ticks)
        {
            g_charge_stale_tick_count++;
        }
        if(p_config->sample_timeout_ticks <= g_charge_stale_tick_count)
        {
            *p_fault = SVC_CHARGE_FAULT_SAMPLE_TIMEOUT;
            return PLATFORM_ERR_TIMEOUT;
        }
    }
    return PLATFORM_ERR_OK;
}

static void process_wait_sample(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint16_t                   duty_permille;
    platform_err_t             status;
    svc_charge_fault_t         fault;

    if(0U == svc_measure_is_ready())
    {
        return;
    }
    status = svc_measure_get(&value);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_MEASUREMENT, status);
        return;
    }
    fault = svc_charge_protection_check_wake(&value);
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        enter_fault(fault, PLATFORM_ERR_HW);
        return;
    }
    fault = svc_charge_protection_update_temperature(&value);
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        enter_fault(fault, PLATFORM_ERR_HW);
        return;
    }
    if(SVC_CHARGE_FAULT_NONE !=
       svc_charge_protection_get_temperature_condition(&value))
    {
        return;
    }
    status = svc_charge_control_calculate_initial_duty(&value, &duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        if(0U == svc_charge_control_is_wake_cv_required(&value))
        {
            enter_fault(SVC_CHARGE_FAULT_DUTY_LIMIT, status);
            return;
        }
        if(p_config->voltage_target_mv >= value.input_voltage_mv)
        {
            enter_fault(SVC_CHARGE_FAULT_INPUT_VOLTAGE, PLATFORM_ERR_HW);
            return;
        }
        duty_permille = BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE;
    }
    else
    {
        fault = svc_charge_protection_check_start_voltage(&value);
        if(SVC_CHARGE_FAULT_NONE != fault)
        {
            enter_fault(fault, PLATFORM_ERR_HW);
            return;
        }
    }
    status = bsp_power_path_charge_start(duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille     = duty_permille;
    g_charge_duty_high_limited = 0U;
    g_charge_last_sequence     = svc_measure_get_sequence();
    g_charge_stale_tick_count  = 0U;
    g_charge_settle_tick_count = 0U;
    g_charge_status            = PLATFORM_ERR_BUSY;
    g_charge_state = (BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE ==
                          duty_permille &&
                      0U != svc_charge_control_is_wake_cv_required(&value))
                         ? SVC_CHARGE_STATE_WAKE_CV
                         : SVC_CHARGE_STATE_START_SETTLE;
}

static void process_start_settle(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint16_t                   duty_permille;
    uint8_t                    is_new;
    svc_charge_fault_t         fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 0U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    fault = svc_charge_protection_check_start_voltage(&value);
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        enter_fault(fault, PLATFORM_ERR_HW);
        return;
    }
    status = svc_charge_control_calculate_initial_duty(&value, &duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_DUTY_LIMIT, status);
        return;
    }
    status = bsp_power_path_charge_set_duty(duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille = duty_permille;
    g_charge_settle_tick_count++;
    if(p_config->start_settle_ticks <= g_charge_settle_tick_count)
    {
        g_charge_current_error_ma = (int32_t)p_config->current_target_ma -
                                    (int32_t)value.input_current_ma;
        g_charge_voltage_error_mv = (int32_t)p_config->voltage_target_mv -
                                    (int32_t)value.battery_voltage_mv;
        g_charge_status           = PLATFORM_ERR_OK;
        if((p_config->voltage_target_mv - p_config->voltage_deadband_mv) <=
           value.battery_voltage_mv)
        {
            enter_constant_voltage(&value);
        }
        else
        {
            g_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
        }
    }
}

static void process_constant_current(void)
{
    const svc_charge_config_t  *p_config = svc_charge_config_get();
    svc_measure_value_t         value;
    svc_charge_control_result_t control_result;
    uint8_t                     is_new;
    svc_charge_fault_t          fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 0U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    if((p_config->voltage_target_mv - p_config->voltage_deadband_mv) <=
       value.battery_voltage_mv)
    {
        enter_constant_voltage(&value);
        return;
    }
    status = calculate_running_duty(&value, g_charge_duty_permille,
                                    &control_result);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_CONFIG, status);
        return;
    }
    g_charge_current_error_ma  = control_result.current_error_ma;
    g_charge_duty_high_limited = control_result.duty_high_limited;
    fault = svc_charge_protection_update_input_loss(&value,
                                                    g_charge_duty_high_limited);
    if(SVC_CHARGE_FAULT_NONE == fault)
    {
        /* Direct P&O may not reach maximum duty after loss of input power. */
        fault = svc_charge_protection_update_maintenance_reverse(&value);
    }
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        enter_fault(fault, PLATFORM_ERR_HW);
        return;
    }
    if(0U == control_result.duty_changed)
    {
        return;
    }
    status = bsp_power_path_charge_set_duty(control_result.duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille = control_result.duty_permille;
}

static platform_err_t adjust_cv_duty(const svc_measure_value_t *p_value,
                                     uint32_t            voltage_target_mv,
                                     svc_charge_fault_t *p_fault)
{
    svc_charge_control_result_t control_result;
    platform_err_t              status;

    if(NULL == p_value || NULL == p_fault)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_fault                         = SVC_CHARGE_FAULT_NONE;
    g_charge_cv_duty_ceiling_limited = 0U;
    g_charge_trace_source            = SVC_CHARGE_TRACE_SOURCE_CV;
    status = svc_charge_control_calculate_cv(p_value, g_charge_duty_permille,
                                             voltage_target_mv,
                                             &control_result);
    g_charge_current_error_ma  = control_result.current_error_ma;
    g_charge_voltage_error_mv  = control_result.voltage_error_mv;
    g_charge_duty_high_limited = control_result.duty_high_limited;
    if(PLATFORM_ERR_OK != status)
    {
        *p_fault = SVC_CHARGE_FAULT_DUTY_LIMIT;
        return status;
    }
    if(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state &&
       0U != g_charge_cv_duty_ceiling_active)
    {
        if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE >
               g_charge_cv_duty_ceiling_permille ||
           BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE <
               g_charge_cv_duty_ceiling_permille)
        {
            *p_fault = SVC_CHARGE_FAULT_CONFIG;
            return PLATFORM_ERR_PARAM;
        }
        if(control_result.duty_permille > g_charge_cv_duty_ceiling_permille)
        {
            control_result.duty_permille = g_charge_cv_duty_ceiling_permille;
            control_result.duty_changed  = (control_result.duty_permille !=
                                            g_charge_duty_permille)
                                               ? 1U
                                               : 0U;
            g_charge_cv_duty_ceiling_limited = 1U;
        }
        else if(0U != control_result.duty_high_limited &&
                g_charge_duty_permille == g_charge_cv_duty_ceiling_permille)
        {
            g_charge_cv_duty_ceiling_limited = 1U;
        }
    }
    if(0U == control_result.duty_changed)
    {
        return PLATFORM_ERR_OK;
    }
    status = bsp_power_path_charge_set_duty(control_result.duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        *p_fault = SVC_CHARGE_FAULT_POWER_PATH;
        return status;
    }
    g_charge_duty_permille = control_result.duty_permille;
    return PLATFORM_ERR_OK;
}

static void enter_full_wait(void)
{
    platform_err_t status = bsp_power_path_charge_stop();

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille     = 0U;
    g_charge_duty_high_limited = 0U;
    reset_mppt_runtime();
    g_charge_status = PLATFORM_ERR_OK;
    g_charge_state  = SVC_CHARGE_STATE_FULL_WAIT;
}

static void process_wake_cv(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint8_t                    is_new;
    svc_charge_fault_t         fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 1U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    status = adjust_cv_duty(&value, p_config->voltage_target_mv, &fault);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    g_charge_status = PLATFORM_ERR_OK;
    if((p_config->voltage_target_mv - p_config->voltage_deadband_mv) >
           value.battery_voltage_mv &&
       (p_config->current_target_ma - p_config->current_deadband_ma) <=
           value.input_current_ma)
    {
        reset_mppt_runtime();
        g_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
    }
}

static void process_constant_voltage(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint8_t                    is_new;
    svc_charge_fault_t         fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 0U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    if((p_config->voltage_target_mv - p_config->cv_exit_margin_mv) >=
       value.battery_voltage_mv)
    {
        svc_charge_maintenance_reset();
        reset_mppt_runtime();
        g_charge_current_error_ma = (int32_t)p_config->current_target_ma -
                                    (int32_t)value.input_current_ma;
        g_charge_voltage_error_mv = (int32_t)p_config->voltage_target_mv -
                                    (int32_t)value.battery_voltage_mv;
        g_charge_state            = SVC_CHARGE_STATE_CONSTANT_CURRENT;
        return;
    }
    status = adjust_cv_duty(&value, p_config->voltage_target_mv, &fault);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    fault = svc_charge_protection_update_input_loss(&value,
                                                    g_charge_duty_high_limited);
    if(SVC_CHARGE_FAULT_NONE != fault)
    {
        enter_fault(fault, PLATFORM_ERR_HW);
        return;
    }
    if(0U != svc_charge_maintenance_update_completion(&value))
    {
        enter_full_wait();
    }
}

static void process_full_wait(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint16_t                   duty_permille;
    uint8_t                    is_new;
    svc_charge_fault_t         fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 0U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    status = svc_charge_maintenance_calculate_start_duty(&value,
                                                         &duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        return;
    }
    status = bsp_power_path_charge_start(duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille     = duty_permille;
    g_charge_duty_high_limited = 0U;
    g_charge_current_error_ma  = (int32_t)p_config->current_target_ma -
                                 (int32_t)value.input_current_ma;
    g_charge_voltage_error_mv  = (int32_t)
                                     p_config->maintenance.voltage_target_mv -
                                 (int32_t)value.battery_voltage_mv;
    g_charge_status            = PLATFORM_ERR_OK;
    g_charge_state             = SVC_CHARGE_STATE_MAINTENANCE;
}

static void process_maintenance(void)
{
    svc_charge_control_result_t control_result;
    svc_measure_value_t         value;
    uint8_t                     is_new;
    svc_charge_fault_t          fault;
    platform_err_t status = get_new_measurement(&value, &is_new, &fault, 0U);

    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    if(0U == is_new)
    {
        return;
    }
    g_charge_trace_source = SVC_CHARGE_TRACE_SOURCE_MAINTENANCE;
    status = svc_charge_maintenance_calculate_cv(&value, g_charge_duty_permille,
                                                 &control_result, &fault);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(fault, status);
        return;
    }
    g_charge_current_error_ma  = control_result.current_error_ma;
    g_charge_voltage_error_mv  = control_result.voltage_error_mv;
    g_charge_duty_high_limited = control_result.duty_high_limited;
    if(0U != svc_charge_maintenance_update_recharge_request(&value))
    {
        enter_fault(SVC_CHARGE_FAULT_INPUT_VOLTAGE, PLATFORM_ERR_HW);
        return;
    }
    if(0U == control_result.duty_changed)
    {
        return;
    }
    status = bsp_power_path_charge_set_duty(control_result.duty_permille);
    if(PLATFORM_ERR_OK != status)
    {
        enter_fault(SVC_CHARGE_FAULT_POWER_PATH, status);
        return;
    }
    g_charge_duty_permille = control_result.duty_permille;
}

static void return_to_wait_sample(void)
{
    svc_charge_protection_reset();
    svc_charge_maintenance_reset();
    reset_mppt_runtime();
    g_charge_recovery_tick_count = 0U;
    g_charge_recovery_allowed    = 0U;
    g_charge_recovery_armed      = 0U;
    g_charge_stale_tick_count    = 0U;
    g_charge_fault               = SVC_CHARGE_FAULT_NONE;
    g_charge_status              = PLATFORM_ERR_BUSY;
    g_charge_state               = SVC_CHARGE_STATE_WAIT_SAMPLE;
}

static void process_fault_recovery(void)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_measure_value_t        value;
    uint32_t                   sequence;
    uint32_t                   healthy_confirm_ticks;
    platform_err_t             status;

    if(0U == g_charge_recovery_allowed)
    {
        return;
    }
    if(0U == g_charge_recovery_armed)
    {
        status = bsp_power_path_charge_stop();
        if(PLATFORM_ERR_OK != status)
        {
            g_charge_status = status;
            return;
        }
        g_charge_duty_permille       = 0U;
        g_charge_recovery_tick_count = 0U;
        g_charge_recovery_armed      = 1U;
        g_charge_last_sequence       = svc_measure_get_sequence();
        return;
    }

    if(SVC_CHARGE_FAULT_CONFIG == g_charge_fault &&
       0U == svc_charge_protection_is_recovery_condition_met(g_charge_fault,
                                                             NULL))
    {
        g_charge_recovery_tick_count = 0U;
        return;
    }
    status = svc_measure_get(&value);
    if(PLATFORM_ERR_OK != status)
    {
        g_charge_recovery_tick_count = 0U;
        return;
    }
    sequence = svc_measure_get_sequence();
    if(sequence == g_charge_last_sequence)
    {
        return;
    }
    g_charge_last_sequence = sequence;
    if(0U != svc_charge_protection_is_recovery_condition_met(g_charge_fault,
                                                             &value) &&
       0U != is_start_condition_valid(&value))
    {
        g_charge_recovery_tick_count++;
    }
    else
    {
        g_charge_recovery_tick_count = 0U;
    }
    healthy_confirm_ticks = delay_ms_to_ticks(
        (SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == g_charge_fault)
            ? p_config->recovery.overcurrent_delay_ms
            : p_config->recovery.normal_delay_ms);
    if(healthy_confirm_ticks <= g_charge_recovery_tick_count)
    {
        return_to_wait_sample();
    }
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_charge_init(svc_charge_battery_system_t system)
{
    platform_err_t config_status = svc_charge_config_init(system);
    platform_err_t stop_status;

    reset_trace();
    svc_charge_protection_reset();
    svc_charge_maintenance_reset();
    reset_mppt_runtime();
    g_charge_duty_permille     = 0U;
    g_charge_stale_tick_count  = 0U;
    g_charge_settle_tick_count = 0U;
    g_charge_current_error_ma  = 0;
    g_charge_voltage_error_mv  = 0;
    reset_recovery_runtime();
    g_charge_fault_snapshot_valid = 0U;
    g_charge_duty_high_limited    = 0U;
    stop_status                   = bsp_power_path_charge_stop();
    if(PLATFORM_ERR_OK != stop_status)
    {
        g_charge_status = stop_status;
        g_charge_fault  = SVC_CHARGE_FAULT_POWER_PATH;
        g_charge_state  = SVC_CHARGE_STATE_FAULT;
    }
    else if(PLATFORM_ERR_OK != config_status)
    {
        g_charge_status = config_status;
        g_charge_fault  = SVC_CHARGE_FAULT_CONFIG;
        g_charge_state  = SVC_CHARGE_STATE_FAULT;
    }
    else
    {
        g_charge_status = PLATFORM_ERR_OK;
        g_charge_fault  = SVC_CHARGE_FAULT_NONE;
        g_charge_state  = SVC_CHARGE_STATE_OFF;
    }
    return g_charge_status;
}

platform_err_t svc_charge_start(void)
{
    if(SVC_CHARGE_STATE_OFF != g_charge_state)
    {
        return PLATFORM_ERR_BUSY;
    }
    svc_charge_protection_reset();
    svc_charge_maintenance_reset();
    reset_mppt_runtime();
    g_charge_stale_tick_count  = 0U;
    g_charge_settle_tick_count = 0U;
    g_charge_current_error_ma  = 0;
    g_charge_voltage_error_mv  = 0;
    reset_recovery_runtime();
    g_charge_fault_snapshot_valid = 0U;
    g_charge_duty_high_limited    = 0U;
    g_charge_fault                = SVC_CHARGE_FAULT_NONE;
    g_charge_status               = PLATFORM_ERR_BUSY;
    g_charge_state                = SVC_CHARGE_STATE_WAIT_SAMPLE;
    return PLATFORM_ERR_OK;
}

void svc_charge_process(void)
{
    svc_charge_trace_snapshot_t snapshot;

    if(0U != g_charge_trace_clear_request)
    {
        reset_trace();
    }
    g_charge_trace_service_tick++;
    g_charge_trace_measure_valid = 0U;
    g_charge_trace_source        = SVC_CHARGE_TRACE_SOURCE_NONE;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.service_tick        = g_charge_trace_service_tick;
    snapshot.duty_before         = g_charge_duty_permille;
    snapshot.state_before        = g_charge_state;
    snapshot.cc_before           = g_charge_current_limited;
    snapshot.mppt_compare_before = svc_charge_mppt_get_compare_count();
    if(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state)
    {
        process_wait_sample();
    }
    else if(SVC_CHARGE_STATE_START_SETTLE == g_charge_state)
    {
        process_start_settle();
    }
    else if(SVC_CHARGE_STATE_WAKE_CV == g_charge_state)
    {
        process_wake_cv();
    }
    else if(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state)
    {
        process_constant_current();
    }
    else if(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state)
    {
        process_constant_voltage();
    }
    else if(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state)
    {
        process_full_wait();
    }
    else if(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state)
    {
        process_maintenance();
    }
    else if(SVC_CHARGE_STATE_FAULT == g_charge_state)
    {
        process_fault_recovery();
    }
    update_trace(&snapshot);
}

platform_err_t svc_charge_stop(void)
{
    svc_charge_mppt_trace_freeze(SVC_CHARGE_MPPT_TRACE_STOP);
    svc_charge_protection_reset();
    svc_charge_maintenance_reset();
    reset_mppt_runtime();
    g_charge_status            = bsp_power_path_charge_stop();
    g_charge_fault             = (PLATFORM_ERR_OK == g_charge_status)
                                     ? SVC_CHARGE_FAULT_NONE
                                     : SVC_CHARGE_FAULT_POWER_PATH;
    g_charge_state             = (PLATFORM_ERR_OK == g_charge_status)
                                     ? SVC_CHARGE_STATE_OFF
                                     : SVC_CHARGE_STATE_FAULT;
    g_charge_duty_permille     = 0U;
    g_charge_stale_tick_count  = 0U;
    g_charge_settle_tick_count = 0U;
    g_charge_current_error_ma  = 0;
    g_charge_voltage_error_mv  = 0;
    reset_recovery_runtime();
    g_charge_duty_high_limited = 0U;
    return g_charge_status;
}

svc_charge_state_t svc_charge_get_state(void)
{
    return g_charge_state;
}

svc_charge_fault_t svc_charge_get_fault(void)
{
    return g_charge_fault;
}

platform_err_t svc_charge_get_status(void)
{
    return g_charge_status;
}

uint16_t svc_charge_get_duty_permille(void)
{
    return g_charge_duty_permille;
}

platform_err_t svc_charge_set_temperature_config(
    const svc_charge_temperature_config_t *p_config)
{
    platform_err_t status = svc_charge_config_set_temperature(p_config);

    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    svc_charge_protection_reset();
    return PLATFORM_ERR_OK;
}

platform_err_t
svc_charge_get_temperature_config(svc_charge_temperature_config_t *p_config)
{
    return svc_charge_config_get_temperature(p_config);
}

/* end of file --------------------------------------------------------------*/
