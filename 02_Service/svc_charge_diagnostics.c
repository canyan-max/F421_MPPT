/**
 ***************************************************************************************************
 *@file               :   svc_charge_diagnostics.c
 *@brief              :   Charge observations and fault snapshots.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* includes --------------------------------------------------------------------------------------*/
#include "svc_charge_diagnostics.h"
#include "svc_charge_control.h"
#include "bsp_power_path.h"
/* define   --------------------------------------------------------------------------------------*/

/* typedef ---------------------------------------------------------------------------------------*/

/* variables -------------------------------------------------------------------------------------*/
svc_charge_cv_observation_t g_charge_cv_observation;
int32_t                     g_charge_current_error_ma;
int32_t                     g_charge_voltage_error_mv;
uint8_t                     g_charge_input_limited;
uint8_t                     g_charge_current_limited;
svc_measure_value_t         g_charge_fault_measure_snapshot;
svc_charge_state_t          g_charge_fault_state_snapshot;
uint16_t                    g_charge_fault_duty_snapshot;
uint32_t                    g_charge_fault_sequence_snapshot;
uint8_t                     g_charge_fault_snapshot_valid;

/* private  functions  ---------------------------------------------------------------------------*/
static uint16_t get_cv_duty_min(uint16_t reference_permille) {
    if (reference_permille <=
        SVC_CHARGE_CV_DUTY_REFERENCE_MARGIN_PERMILLE + BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE) {
        return BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE;
    }
    return (uint16_t)(reference_permille - SVC_CHARGE_CV_DUTY_REFERENCE_MARGIN_PERMILLE);
}

/* exported functions ----------------------------------------------------------------------------*/
void svc_charge_diagnostics_reset(void) {
    const svc_charge_cv_observation_t observation = {0};

    g_charge_cv_observation   = observation;
    g_charge_current_error_ma = 0;
    g_charge_voltage_error_mv = 0;
    g_charge_input_limited    = 0U;
}

void svc_charge_diagnostics_capture_fault(svc_charge_state_t state, uint16_t duty_permille) {
    svc_measure_value_t value;

    g_charge_fault_state_snapshot    = state;
    g_charge_fault_duty_snapshot     = duty_permille;
    g_charge_fault_sequence_snapshot = svc_measure_get_sequence();
    if (PLATFORM_ERR_OK == svc_measure_get(&value)) {
        g_charge_fault_measure_snapshot = value;
        g_charge_fault_snapshot_valid   = 1U;
    } else {
        g_charge_fault_snapshot_valid = 0U;
    }
}

void svc_charge_diagnostics_observe_cv(const svc_measure_value_t *p_value, uint16_t duty_permille) {
    const svc_charge_config_t  *p_config    = svc_charge_config_get();
    svc_charge_cv_observation_t observation = {0};
    uint64_t                    minimum_input_mv;
    platform_err_t              status;

    if (NULL == p_value) {
        g_charge_cv_observation.valid = 0U;
        return;
    }
    status = svc_charge_control_calculate_duty_reference(p_value,
                                                         &observation.duty_reference_permille);
    if (PLATFORM_ERR_OK != status) {
        g_charge_cv_observation.valid = 0U;
        return;
    }
    /* Ceil(Vbat * scale / Dmax) is voltage headroom, not a PV power limit.
     * Keep this fresh calculation separate from the existing guard's cache. */
    minimum_input_mv = ((uint64_t)p_value->battery_voltage_mv * p_config->duty_scale +
                        BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE - 1U) /
                       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE;
    if (UINT32_MAX < minimum_input_mv) {
        g_charge_cv_observation.valid = 0U;
        return;
    }
    observation.sequence                = svc_measure_get_sequence();
    observation.input_voltage_mv        = p_value->input_voltage_mv;
    observation.battery_voltage_mv      = p_value->battery_voltage_mv;
    observation.total_output_current_ma = p_value->input_current_ma;
    observation.input_min_reference_mv  = (uint32_t)minimum_input_mv;
    observation.duty_permille           = duty_permille;
    observation.duty_min_permille       = get_cv_duty_min(observation.duty_reference_permille);
    observation.duty_gap_permille       = (int32_t)duty_permille -
                                          (int32_t)observation.duty_reference_permille;
    /* Within one existing control step, or already below the reference.
     * 100 mA is an observation threshold selected by the user, not a trip. */
    observation.near_reference = (observation.duty_gap_permille <=
                                  (int32_t)p_config->control_step_permille)
                                     ? 1U
                                     : 0U;
    observation.low_current    = (SVC_CHARGE_CV_OBSERVE_LOW_CURRENT_MA >= p_value->input_current_ma)
                                     ? 1U
                                     : 0U;
    observation.low_power_boundary = (0U != observation.near_reference &&
                                      0U != observation.low_current)
                                         ? 1U
                                         : 0U;
    observation.input_headroom_low = (p_value->input_voltage_mv <=
                                      observation.input_min_reference_mv)
                                         ? 1U
                                         : 0U;
    observation.valid              = 1U;
    g_charge_cv_observation        = observation;
}

/* end of file -----------------------------------------------------------------------------------*/
