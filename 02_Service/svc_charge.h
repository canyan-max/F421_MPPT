/**
 ***************************************************************************************************
 *@file               :   svc_charge.h
 *@brief              :   Start the Buck power path after validated sampling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef SVC_CHARGE_H
#define SVC_CHARGE_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_charge_config.h"
#include "svc_charge_protection.h"

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/
#define SVC_CHARGE_CV_DUTY_REFERENCE_MARGIN_PERMILLE (0U)

/* First-board-test values; Voc is estimated from PWM-off startup sampling. */
#define SVC_CHARGE_INPUT_VOC_RATIO_PERMILLE (875U)
#define SVC_CHARGE_CV_INPUT_RECOVER_HYSTERESIS_MV (200U)

/* typedef ---------------------------------------------------------------------------------------*/
typedef enum SVC_CHARGE_STATE_T {
    SVC_CHARGE_STATE_OFF = 0U,
    SVC_CHARGE_STATE_WAIT_SAMPLE,
    SVC_CHARGE_STATE_START_SETTLE,
    SVC_CHARGE_STATE_WAKE_CV          = 3U,
    SVC_CHARGE_STATE_CONSTANT_CURRENT = 4U,
    SVC_CHARGE_STATE_CONSTANT_VOLTAGE = 5U,
    SVC_CHARGE_STATE_FAULT            = 6U,
    SVC_CHARGE_STATE_FULL_WAIT        = 7U, /* Charged standby, PWM off. */
    SVC_CHARGE_STATE_MAINTENANCE      = 8U,
    SVC_CHARGE_STATE_MPPT             = 9U,
} svc_charge_state_t;

/* variables -------------------------------------------------------------------------------------*/
/* Rounded current Vbat / saved startup Voc lower limit, permille.
 * A theoretical reverse-voltage equilibrium reference only. CC/CV/MPPT
 * normalize both applied duty after voltage drift and outgoing candidates to
 * this limit. A reference above 950 faults; startup keeps its checked offset.
 * Fault retains value; stop/restart clears to zero. The ideal ratio is a
 * software constraint, not a verified reverse-current safety boundary.
 */
extern uint16_t g_charge_duty_min_reference_permille;
/* Startup PWM-off Vin estimate, held until next startup (not verified Voc).
 * Input reference = estimate * 875/1000, used only for CV input regulation.
 * An empty interval faults before any PWM update; recovery re-samples Voc.
 * range_conflict is retained through fault recovery, cleared at next startup.
 */
extern uint32_t g_charge_input_voc_mv;
extern uint32_t g_charge_input_reference_mv;
/* Compatibility Watch name: actual fixed upper limit, now 950 while running. */
extern uint16_t g_charge_input_duty_max_permille;
extern uint8_t  g_charge_input_range_conflict;
/* CV input constraint latch: below reference reduces D; recovery band holds;
 * reference + trial hysteresis releases. Not a main state or fault flag. */
extern uint8_t g_charge_input_limited;
/* function  -------------------------------------------------------------------------------------*/
/* Forces the charge path off and resets the startup state. */
platform_err_t svc_charge_init(svc_charge_battery_system_t system);
/* Arms one automatic start after the measurement service becomes ready. */
platform_err_t svc_charge_start(void);
/* Nonblocking processing. Call exactly once per SVC_CHARGE_PROCESS_PERIOD_MS.
 */
void svc_charge_process(void);
/* Stops charging. A subsequent start call is required to re-arm. */
platform_err_t     svc_charge_stop(void);
svc_charge_state_t svc_charge_get_state(void);
svc_charge_fault_t svc_charge_get_fault(void);
platform_err_t     svc_charge_get_status(void);
uint16_t           svc_charge_get_duty_permille(void);
/* Main context only. The complete configuration is validated then copied. */
platform_err_t svc_charge_set_temperature_config(const svc_charge_temperature_config_t *p_config);
platform_err_t svc_charge_get_temperature_config(svc_charge_temperature_config_t *p_config);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_H */
/* end of file -----------------------------------------------------------------------------------*/
