/**
 ******************************************************************************
 *@file               :   svc_led.c
 *@brief              :   Translate operating status into three steady LEDs.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_led.h"
#include "svc_charge.h"
#include "svc_discharge.h"
#include "svc_measure.h"
#include "bsp_led.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/
static uint8_t is_charging(void)
{
    if(SVC_CHARGE_FAULT_NONE != svc_charge_get_fault() ||
       0U == svc_charge_get_duty_permille())
    {
        return 0U;
    }
    switch(svc_charge_get_state())
    {
        case SVC_CHARGE_STATE_WAKE_CV:
        case SVC_CHARGE_STATE_CONSTANT_CURRENT:
        case SVC_CHARGE_STATE_CONSTANT_VOLTAGE:
        case SVC_CHARGE_STATE_MAINTENANCE:
            return 1U;
        default:
            return 0U;
    }
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_led_init(void)
{
    return bsp_led_set(0U, 0U, 0U);
}

platform_err_t svc_led_process(void)
{
    const svc_discharge_config_t *p_config = svc_discharge_get_config();
    svc_measure_value_t           value;
    platform_err_t                status = svc_measure_get(&value);
    uint8_t                       sample_valid;
    uint8_t                       charge_on;
    uint8_t                       battery_on;
    uint8_t                       load_on;

    sample_valid = PLATFORM_ERR_OK == status && 0U != svc_measure_is_ready() &&
                   SVC_MEASURE_TEMPERATURE_INVALID != value.temperature_deci_c;
    charge_on    = 0U != sample_valid && 0U != is_charging();
    /* Reuse undervoltage hysteresis/recovery; manual, load current and thermal
     * shutdown do not by themselves mean the battery voltage is abnormal. */
    battery_on = 0U != sample_valid && NULL != p_config &&
                 0U != p_config->voltage_trip_mv &&
                 value.battery_voltage_mv > p_config->voltage_trip_mv &&
                 0U == (g_discharge_fault_flags &
                        (SVC_DISCHARGE_FAULT_BATTERY_LOW |
                         SVC_DISCHARGE_FAULT_MEASUREMENT |
                         SVC_DISCHARGE_FAULT_CONFIG));
    load_on    = 0U != sample_valid && 0U != g_discharge_manual_enabled &&
                 SVC_DISCHARGE_STATE_ON == g_discharge_state &&
                 0U != g_discharge_output_enabled &&
                 SVC_DISCHARGE_FAULT_NONE == g_discharge_fault_flags;
    return bsp_led_set(charge_on, battery_on, load_on);
}

/* end of file --------------------------------------------------------------*/
