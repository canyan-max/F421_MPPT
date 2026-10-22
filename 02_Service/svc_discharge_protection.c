/**
 ******************************************************************************
 *@file               :   svc_discharge_protection.c
 *@brief              :   Evaluate discharge faults and protection timing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_discharge_protection.h"
#include "svc_discharge_config.h"
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_DISCHARGE_GUARD_T
{
    SVC_DISCHARGE_GUARD_LOW_TEMP = 0U,
    SVC_DISCHARGE_GUARD_HIGH_TEMP,
    SVC_DISCHARGE_GUARD_LOW_BATTERY,
    SVC_DISCHARGE_GUARD_OVERCURRENT,
    SVC_DISCHARGE_GUARD_NUM,
} svc_discharge_guard_t;

/* variables ----------------------------------------------------------------*/
uint32_t        g_discharge_fault_flags;
static uint16_t g_discharge_trip_ticks[SVC_DISCHARGE_GUARD_NUM];
static uint16_t g_discharge_recover_ticks[SVC_DISCHARGE_GUARD_NUM];
static uint16_t g_discharge_valid_ticks;
static uint16_t g_discharge_stale_ticks;
static uint16_t g_discharge_gpio_recover_ticks;
static uint32_t g_discharge_last_sequence;
static uint8_t  g_discharge_has_sequence;
static uint32_t g_discharge_short_generation;
static uint16_t g_discharge_short_retry_ticks;
static uint8_t  g_discharge_short_seen;

/* private  functions  ------------------------------------------------------*/
static void clear_confirmation_ticks(void)
{
    uint32_t index;

    g_discharge_valid_ticks        = 0U;
    g_discharge_gpio_recover_ticks = 0U;
    for(index = 0U; index < SVC_DISCHARGE_GUARD_NUM; index++)
    {
        g_discharge_trip_ticks[index]    = 0U;
        g_discharge_recover_ticks[index] = 0U;
    }
}

static platform_err_t measurement_fault(platform_err_t status)
{
    clear_confirmation_ticks();
    g_discharge_fault_flags |= SVC_DISCHARGE_FAULT_MEASUREMENT;
    return status;
}

static void update_guard(svc_discharge_guard_t guard,
                         uint32_t              flag,
                         uint8_t               trip,
                         uint8_t               recover)
{
    const svc_discharge_config_t *p_config = svc_discharge_config_get();
    uint16_t                      recover_ticks;

    if(guard >= SVC_DISCHARGE_GUARD_NUM)
    {
        return;
    }
    recover_ticks = (SVC_DISCHARGE_GUARD_OVERCURRENT == guard)
                        ? p_config->overcurrent_recover_ticks
                        : p_config->recover_ticks;
    if(0U == (g_discharge_fault_flags & flag))
    {
        g_discharge_recover_ticks[guard] = 0U;
        if(0U == trip)
        {
            g_discharge_trip_ticks[guard] = 0U;
        }
        else if(++g_discharge_trip_ticks[guard] >= p_config->trip_ticks)
        {
            g_discharge_fault_flags |= flag;
            g_discharge_trip_ticks[guard] = 0U;
        }
    }
    else
    {
        g_discharge_trip_ticks[guard] = 0U;
        if(0U == recover)
        {
            g_discharge_recover_ticks[guard] = 0U;
        }
        else if(++g_discharge_recover_ticks[guard] >= recover_ticks)
        {
            g_discharge_fault_flags &= ~flag;
            g_discharge_recover_ticks[guard] = 0U;
        }
    }
}

static uint8_t update_conditions(const svc_measure_value_t *p_value)
{
    const svc_discharge_config_t *p_config = svc_discharge_config_get();
    uint8_t                       low_temp;
    uint8_t                       high_temp;
    uint8_t                       low_battery;
    uint8_t                       overcurrent;

    if(NULL == p_value)
    {
        return 1U;
    }
    low_temp    = (p_value->temperature_deci_c <= p_config->low_trip_deci_c);
    high_temp   = (p_value->temperature_deci_c >= p_config->high_trip_deci_c);
    low_battery = (p_value->battery_voltage_mv <= p_config->voltage_trip_mv);
    overcurrent = (p_value->output_current_ma >= p_config->current_trip_ma);
    update_guard(SVC_DISCHARGE_GUARD_LOW_TEMP,
                 SVC_DISCHARGE_FAULT_TEMPERATURE_LOW, low_temp,
                 p_value->temperature_deci_c >= p_config->low_recover_deci_c);
    update_guard(SVC_DISCHARGE_GUARD_HIGH_TEMP,
                 SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH, high_temp,
                 p_value->temperature_deci_c <= p_config->high_recover_deci_c);
    update_guard(SVC_DISCHARGE_GUARD_LOW_BATTERY,
                 SVC_DISCHARGE_FAULT_BATTERY_LOW, low_battery,
                 p_value->battery_voltage_mv >= p_config->voltage_recover_mv);
    update_guard(SVC_DISCHARGE_GUARD_OVERCURRENT,
                 SVC_DISCHARGE_FAULT_OVERCURRENT, overcurrent,
                 p_value->output_current_ma < p_config->current_trip_ma);
    return low_temp || high_temp || low_battery || overcurrent;
}

/* exported functions -------------------------------------------------------*/
void svc_discharge_protection_reset(void)
{
    g_discharge_fault_flags       = SVC_DISCHARGE_FAULT_MEASUREMENT;
    g_discharge_has_sequence      = 0U;
    g_discharge_stale_ticks       = 0U;
    g_discharge_short_seen        = 0U;
    g_discharge_short_retry_ticks = 0U;
    clear_confirmation_ticks();
}

void svc_discharge_protection_latch_fault(svc_discharge_fault_t fault)
{
    g_discharge_fault_flags |= fault;
    if(0U != (fault & SVC_DISCHARGE_FAULT_GPIO))
    {
        g_discharge_gpio_recover_ticks = 0U;
    }
}

uint8_t svc_discharge_protection_update_short(void)
{
    const svc_discharge_config_t *p_config = svc_discharge_config_get();
    bsp_discharge_short_state_t   state = bsp_power_path_discharge_short_get();

    if(0U == state.latched)
    {
        return 0U;
    }
    g_discharge_fault_flags |= SVC_DISCHARGE_FAULT_SHORT_CIRCUIT;
    /* The ISR may have changed PB11 since the last service command. */
    if(0U == g_discharge_short_seen ||
       state.generation != g_discharge_short_generation)
    {
        g_discharge_short_seen        = 1U;
        g_discharge_short_generation  = state.generation;
        g_discharge_short_retry_ticks = 0U;
    }
    else if(g_discharge_short_retry_ticks < p_config->short_retry_ticks)
    {
        g_discharge_short_retry_ticks++;
    }
    if(g_discharge_short_retry_ticks >= p_config->short_retry_ticks &&
       PLATFORM_ERR_OK ==
           bsp_power_path_discharge_short_rearm(state.generation))
    {
        g_discharge_fault_flags &= ~SVC_DISCHARGE_FAULT_SHORT_CIRCUIT;
        g_discharge_short_seen = 0U;
    }
    return 1U;
}

platform_err_t svc_discharge_protection_check_sample(
    platform_err_t status, const svc_measure_value_t *p_value, uint8_t *p_fresh)
{
    const svc_discharge_config_t *p_config = svc_discharge_config_get();
    uint32_t                      sequence;

    if(NULL == p_fresh)
    {
        return measurement_fault(PLATFORM_ERR_PARAM);
    }
    *p_fresh = 0U;
    if(PLATFORM_ERR_OK != status)
    {
        return measurement_fault(status);
    }
    if(NULL == p_value)
    {
        return measurement_fault(PLATFORM_ERR_PARAM);
    }
    if(0U == svc_measure_is_ready() ||
       SVC_MEASURE_TEMPERATURE_INVALID == p_value->temperature_deci_c)
    {
        return measurement_fault(PLATFORM_ERR_BUSY);
    }
    sequence = svc_measure_get_sequence();
    if(0U != g_discharge_has_sequence && sequence == g_discharge_last_sequence)
    {
        if(g_discharge_stale_ticks < p_config->sample_timeout_ticks)
        {
            g_discharge_stale_ticks++;
        }
        /* Never accumulate recovery or protection confirmation on old data. */
        clear_confirmation_ticks();
        if(g_discharge_stale_ticks >= p_config->sample_timeout_ticks)
        {
            return measurement_fault(PLATFORM_ERR_TIMEOUT);
        }
        return PLATFORM_ERR_OK;
    }
    g_discharge_has_sequence  = 1U;
    g_discharge_last_sequence = sequence;
    g_discharge_stale_ticks   = 0U;
    *p_fresh                  = 1U;
    return PLATFORM_ERR_OK;
}

uint8_t svc_discharge_protection_update(const svc_measure_value_t *p_value,
                                        uint8_t output_off_confirmed)
{
    const svc_discharge_config_t *p_config = svc_discharge_config_get();
    uint8_t                       pending;

    if(NULL == p_value)
    {
        clear_confirmation_ticks();
        svc_discharge_protection_latch_fault(SVC_DISCHARGE_FAULT_MEASUREMENT);
        return 1U;
    }
    pending = update_conditions(p_value);
    if(g_discharge_valid_ticks < p_config->recover_ticks)
    {
        g_discharge_valid_ticks++;
    }
    if(g_discharge_valid_ticks >= p_config->recover_ticks)
    {
        g_discharge_fault_flags &= ~SVC_DISCHARGE_FAULT_MEASUREMENT;
    }
    if(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_GPIO))
    {
        if(0U != output_off_confirmed)
        {
            if(++g_discharge_gpio_recover_ticks >= p_config->recover_ticks)
            {
                g_discharge_fault_flags &= ~SVC_DISCHARGE_FAULT_GPIO;
                g_discharge_gpio_recover_ticks = 0U;
            }
        }
        else
        {
            g_discharge_gpio_recover_ticks = 0U;
        }
    }
    return pending;
}

/* end of file --------------------------------------------------------------*/
