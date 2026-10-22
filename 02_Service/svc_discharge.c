/**
 ******************************************************************************
 *@file               :   svc_discharge.c
 *@brief              :   Schedule the independent protected load-switch flow.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_discharge.h"
#include "svc_measure.h"
#include "bsp_power_path.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
volatile uint8_t      g_discharge_manual_enabled = 1U;
uint8_t               g_discharge_output_enabled;
svc_discharge_state_t g_discharge_state;
static platform_err_t g_discharge_status = PLATFORM_ERR_BUSY;
static uint8_t        g_discharge_output_known;
static uint8_t        g_discharge_initialized;

/* private  functions  ------------------------------------------------------*/
static platform_err_t set_output(uint8_t enabled)
{
    platform_err_t status;

    if(0U != g_discharge_output_known && enabled == g_discharge_output_enabled)
    {
        return PLATFORM_ERR_OK;
    }
    status = bsp_power_path_discharge_set(enabled);
    if(PLATFORM_ERR_BUSY == status && 0U != enabled)
    {
        /* The BSP has commanded LOW; this is an interlock, not GPIO failure. */
        g_discharge_output_enabled = 0U;
        g_discharge_output_known   = 1U;
        svc_discharge_protection_latch_fault(SVC_DISCHARGE_FAULT_SHORT_CIRCUIT);
        g_discharge_status = status;
        g_discharge_state  = SVC_DISCHARGE_STATE_PROTECTED;
        return status;
    }
    if(PLATFORM_ERR_OK != status)
    {
        g_discharge_output_known = 0U;
        svc_discharge_protection_latch_fault(SVC_DISCHARGE_FAULT_GPIO);
        g_discharge_status = status;
        g_discharge_state  = SVC_DISCHARGE_STATE_FAULT;
        return status;
    }
    g_discharge_output_enabled = enabled;
    g_discharge_output_known   = 1U;
    return PLATFORM_ERR_OK;
}

/* A stale sample holds the current command until its timeout, but cannot
 * advance protection/recovery timers. Errors request OFF immediately. */
static uint8_t update_sample_protection(uint8_t *p_pending)
{
    svc_measure_value_t value;
    platform_err_t      status;
    uint8_t             fresh;

    if(NULL == p_pending)
    {
        return 0U;
    }
    status = svc_measure_get(&value);
    status = svc_discharge_protection_check_sample(status, &value, &fresh);
    if(PLATFORM_ERR_OK != status)
    {
        g_discharge_status = status;
        g_discharge_state  = (0U == g_discharge_manual_enabled)
                                 ? SVC_DISCHARGE_STATE_MANUAL_OFF
                                 : SVC_DISCHARGE_STATE_WAIT_SAMPLE;
    }
    if(PLATFORM_ERR_OK != status ||
       (0U == fresh &&
        0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_GPIO)))
    {
        if(PLATFORM_ERR_OK != set_output(0U))
        {
            return 0U;
        }
    }
    if(PLATFORM_ERR_OK != status || 0U == fresh)
    {
        return 0U;
    }
    *p_pending = svc_discharge_protection_update(
        &value,
        0U != g_discharge_output_known && 0U == g_discharge_output_enabled);
    return 1U;
}

/* Output errors are published by set_output(); this is the normal-state
 * priority after a successful command. */
static svc_discharge_state_t get_runtime_state(uint8_t enabled)
{
    if(0U != (g_discharge_fault_flags &
              (SVC_DISCHARGE_FAULT_GPIO | SVC_DISCHARGE_FAULT_CONFIG)))
    {
        return SVC_DISCHARGE_STATE_FAULT;
    }
    if(0U == g_discharge_manual_enabled)
    {
        return SVC_DISCHARGE_STATE_MANUAL_OFF;
    }
    if(0U != enabled)
    {
        return SVC_DISCHARGE_STATE_ON;
    }
    return (SVC_DISCHARGE_FAULT_MEASUREMENT == g_discharge_fault_flags)
               ? SVC_DISCHARGE_STATE_WAIT_SAMPLE
               : SVC_DISCHARGE_STATE_PROTECTED;
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_discharge_init(svc_discharge_battery_system_t system)
{
    platform_err_t status;
    platform_err_t config_status;

    g_discharge_initialized    = 0U;
    g_discharge_manual_enabled = 1U;
    g_discharge_state          = SVC_DISCHARGE_STATE_WAIT_SAMPLE;
    g_discharge_status         = PLATFORM_ERR_BUSY;
    g_discharge_output_known   = 0U;
    svc_discharge_protection_reset();
    config_status = bsp_power_path_discharge_init();
    status        = set_output(0U);
    if(PLATFORM_ERR_OK == config_status)
    {
        config_status = svc_discharge_config_init(system);
    }
    if(PLATFORM_ERR_OK != config_status)
    {
        svc_discharge_protection_latch_fault(SVC_DISCHARGE_FAULT_CONFIG);
        g_discharge_state  = SVC_DISCHARGE_STATE_FAULT;
        g_discharge_status = (PLATFORM_ERR_OK == status) ? config_status
                                                         : status;
        return g_discharge_status;
    }
    g_discharge_initialized = 1U;
    return status;
}

void svc_discharge_process(void)
{
    uint8_t pending;
    uint8_t enabled;

    if(0U == g_discharge_initialized)
    {
        if(PLATFORM_ERR_OK != set_output(0U))
        {
            return;
        }
        return;
    }
    /* 1. Sync the ISR/BSP latch and advance the independent retry timer. */
    if(0U != svc_discharge_protection_update_short())
    {
        g_discharge_output_known = 0U;
    }
    /* 2. Apply OFF requests before reading samples; keep monitoring while OFF.
     */
    if(0U == g_discharge_manual_enabled ||
       0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_SHORT_CIRCUIT))
    {
        if(PLATFORM_ERR_OK != set_output(0U))
        {
            return;
        }
    }
    if(0U == g_discharge_manual_enabled)
    {
        g_discharge_state = SVC_DISCHARGE_STATE_MANUAL_OFF;
    }

    /* 3. Only a fresh valid sample advances the sampled protection timers. */
    if(0U == update_sample_protection(&pending))
    {
        return;
    }

    /* 4. Pending trips may keep an already-ON load running, never start one. */
    enabled = 0U != g_discharge_manual_enabled &&
              SVC_DISCHARGE_FAULT_NONE == g_discharge_fault_flags &&
              (0U == pending || (0U != g_discharge_output_known &&
                                 0U != g_discharge_output_enabled));
    if(PLATFORM_ERR_OK != set_output(enabled))
    {
        return;
    }
    g_discharge_status = PLATFORM_ERR_OK;
    g_discharge_state  = get_runtime_state(enabled);
}

platform_err_t svc_discharge_set_manual_enabled(uint8_t enabled)
{
    if(1U < enabled)
    {
        return PLATFORM_ERR_PARAM;
    }
    g_discharge_manual_enabled = enabled;
    if(0U == enabled)
    {
        g_discharge_state = SVC_DISCHARGE_STATE_MANUAL_OFF;
        return set_output(0U);
    }
    return PLATFORM_ERR_OK;
}

const svc_discharge_config_t *svc_discharge_get_config(void)
{
    return svc_discharge_config_get();
}

/* end of file --------------------------------------------------------------*/
