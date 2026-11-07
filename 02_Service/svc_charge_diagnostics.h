/**
 ***************************************************************************************************
 *@file               :   svc_charge_diagnostics.h
 *@brief              :   Charge observations and fault snapshots.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef SVC_CHARGE_DIAGNOSTICS_H
#define SVC_CHARGE_DIAGNOSTICS_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "svc_charge.h"
#include "svc_measure.h"

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/
#define SVC_CHARGE_CV_OBSERVE_LOW_CURRENT_MA (100U)

/* typedef ---------------------------------------------------------------------------------------*/
/* Last valid CV measurement and its pre-adjustment duty, main context only.
 * Diagnostic thresholds do not grant reverse-current protection or alter PWM.
 */
typedef struct SVC_CHARGE_CV_OBSERVATION_T {
    uint32_t sequence;
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    uint32_t total_output_current_ma;
    uint32_t input_min_reference_mv;
    int32_t  duty_gap_permille;
    uint16_t duty_permille;
    uint16_t duty_reference_permille;
    uint16_t duty_min_permille; /* Vbat/startup Voc reference floor. */
    uint8_t  valid;
    uint8_t  near_reference;
    uint8_t  low_current;
    uint8_t  low_power_boundary;
    uint8_t  input_headroom_low;
    uint8_t  downward_blocked;
} svc_charge_cv_observation_t;

/* variables -------------------------------------------------------------------------------------*/
/* Main-context diagnostics only; these variables do not control PWM. */
extern svc_charge_cv_observation_t g_charge_cv_observation;
extern int32_t                     g_charge_current_error_ma;
extern int32_t                     g_charge_voltage_error_mv;
extern uint8_t                     g_charge_current_limited;
extern svc_measure_value_t         g_charge_fault_measure_snapshot;
extern svc_charge_state_t          g_charge_fault_state_snapshot;
extern uint16_t                    g_charge_fault_duty_snapshot;
extern uint32_t                    g_charge_fault_sequence_snapshot;
extern uint8_t                     g_charge_fault_snapshot_valid;

/* function  -------------------------------------------------------------------------------------*/
/* Clears runtime observations; preserves fault snapshots and the CC flag.
 * Their original init/start and state-transition reset points stay in charge.
 */
void svc_charge_diagnostics_reset(void);
void svc_charge_diagnostics_capture_fault(svc_charge_state_t state, uint16_t duty_permille);
void svc_charge_diagnostics_observe_cv(const svc_measure_value_t *p_value, uint16_t duty_permille);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_DIAGNOSTICS_H */
/* end of file -----------------------------------------------------------------------------------*/
