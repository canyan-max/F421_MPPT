/**
 ******************************************************************************
 *@file               :   svc_charge_protection.c
 *@brief              :   Evaluate Buck charge faults and recovery timing.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_charge_protection.h"
#include "svc_charge_config.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static uint32_t           g_temperature_trip_tick_count;
static svc_charge_fault_t g_temperature_pending_fault;

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

static uint8_t is_temperature_fault(svc_charge_fault_t fault)
{
    return (SVC_CHARGE_FAULT_TEMPERATURE_LOW == fault ||
            SVC_CHARGE_FAULT_TEMPERATURE_HIGH == fault)
               ? 1U
               : 0U;
}

/* exported functions -------------------------------------------------------*/
void svc_charge_protection_reset(void)
{
    g_temperature_trip_tick_count = 0U;
    g_temperature_pending_fault   = SVC_CHARGE_FAULT_NONE;
}

svc_charge_fault_t
svc_charge_protection_check_immediate(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value || 0U == p_value->battery_voltage_mv)
    {
        return SVC_CHARGE_FAULT_MEASUREMENT;
    }
    if(p_value->input_voltage_mv <= p_value->battery_voltage_mv)
    {
        return SVC_CHARGE_FAULT_INPUT_VOLTAGE;
    }
    if(p_config->current_limit_ma <= p_value->input_current_ma)
    {
        return SVC_CHARGE_FAULT_CHARGE_OVERCURRENT;
    }
    if(SVC_MEASURE_TEMPERATURE_INVALID == p_value->temperature_deci_c)
    {
        return SVC_CHARGE_FAULT_TEMPERATURE_SENSOR;
    }
    return SVC_CHARGE_FAULT_NONE;
}

svc_charge_fault_t
svc_charge_protection_check_wake(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value)
    {
        return SVC_CHARGE_FAULT_MEASUREMENT;
    }
    if(0U == p_value->input_voltage_mv)
    {
        return SVC_CHARGE_FAULT_INPUT_VOLTAGE;
    }
    if(0U != p_value->battery_voltage_mv &&
       p_value->input_voltage_mv <= p_value->battery_voltage_mv)
    {
        return SVC_CHARGE_FAULT_INPUT_VOLTAGE;
    }
    if((p_config->voltage_target_mv + p_config->voltage_deadband_mv) <
       p_value->battery_voltage_mv)
    {
        return SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE;
    }
    if(p_config->current_limit_ma <= p_value->input_current_ma)
    {
        return SVC_CHARGE_FAULT_CHARGE_OVERCURRENT;
    }
    if(SVC_MEASURE_TEMPERATURE_INVALID == p_value->temperature_deci_c)
    {
        return SVC_CHARGE_FAULT_TEMPERATURE_SENSOR;
    }
    return SVC_CHARGE_FAULT_NONE;
}

svc_charge_fault_t svc_charge_protection_get_temperature_condition(
    const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value)
    {
        return SVC_CHARGE_FAULT_MEASUREMENT;
    }
    if(p_value->temperature_deci_c <= p_config->temperature.low_trip_deci_c)
    {
        return SVC_CHARGE_FAULT_TEMPERATURE_LOW;
    }
    if(p_value->temperature_deci_c >= p_config->temperature.high_trip_deci_c)
    {
        return SVC_CHARGE_FAULT_TEMPERATURE_HIGH;
    }
    return SVC_CHARGE_FAULT_NONE;
}

svc_charge_fault_t
svc_charge_protection_update_temperature(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();
    svc_charge_fault_t fault = svc_charge_protection_get_temperature_condition(
        p_value);
    uint32_t trip_ticks = delay_ms_to_ticks(
        p_config->temperature.trip_delay_ms);

    if(SVC_CHARGE_FAULT_NONE == fault)
    {
        g_temperature_trip_tick_count = 0U;
        g_temperature_pending_fault   = SVC_CHARGE_FAULT_NONE;
        return SVC_CHARGE_FAULT_NONE;
    }
    if(fault != g_temperature_pending_fault)
    {
        g_temperature_pending_fault   = fault;
        g_temperature_trip_tick_count = 1U;
    }
    else if(g_temperature_trip_tick_count < trip_ticks)
    {
        g_temperature_trip_tick_count++;
    }
    return (trip_ticks <= g_temperature_trip_tick_count)
               ? fault
               : SVC_CHARGE_FAULT_NONE;
}

svc_charge_fault_t
svc_charge_protection_check_start_voltage(const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(NULL == p_value)
    {
        return SVC_CHARGE_FAULT_MEASUREMENT;
    }
    return ((p_config->voltage_target_mv + p_config->voltage_deadband_mv) <
            p_value->battery_voltage_mv)
               ? SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE
               : SVC_CHARGE_FAULT_NONE;
}

uint8_t svc_charge_protection_is_recovery_condition_met(
    svc_charge_fault_t fault, const svc_measure_value_t *p_value)
{
    const svc_charge_config_t *p_config = svc_charge_config_get();

    if(SVC_CHARGE_FAULT_CONFIG == fault)
    {
        return (PLATFORM_ERR_OK == svc_charge_config_validate()) ? 1U : 0U;
    }
    if(NULL == p_value ||
       SVC_MEASURE_TEMPERATURE_INVALID == p_value->temperature_deci_c)
    {
        return 0U;
    }
    if(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == fault)
    {
        return (p_config->voltage_target_mv >= p_value->battery_voltage_mv)
                   ? 1U
                   : 0U;
    }
    if(SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == fault)
    {
        return ((p_config->current_limit_ma - p_config->current_deadband_ma) >
                p_value->input_current_ma)
                   ? 1U
                   : 0U;
    }
    if(0U != is_temperature_fault(fault))
    {
        return (p_config->temperature.low_recover_deci_c <=
                    p_value->temperature_deci_c &&
                p_config->temperature.high_recover_deci_c >=
                    p_value->temperature_deci_c)
                   ? 1U
                   : 0U;
    }
    return 1U;
}

/* end of file --------------------------------------------------------------*/
