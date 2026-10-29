/**
 ******************************************************************************
 *@file               :   svc_charge.h
 *@brief              :   Start the Buck power path after validated sampling.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_H
#define SVC_CHARGE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_charge_config.h"
#include "svc_charge_protection.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SVC_CHARGE_CV_OBSERVE_LOW_CURRENT_MA (100U)
#define SVC_CHARGE_CV_DUTY_REFERENCE_MARGIN_PERMILLE (15U)

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_STATE_T
{
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

/* Last valid CV measurement and its pre-adjustment duty, main context only.
 * Diagnostic thresholds do not grant reverse-current protection or alter PWM.
 */
typedef struct SVC_CHARGE_CV_OBSERVATION_T
{
    uint32_t sequence;
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    uint32_t total_output_current_ma;
    uint32_t input_min_reference_mv;
    int32_t  duty_gap_permille;
    uint16_t duty_permille;
    uint16_t duty_reference_permille;
    uint16_t duty_min_permille; /* CV reference minus trial margin. */
    uint8_t  valid;
    uint8_t  near_reference;
    uint8_t  low_current;
    uint8_t  low_power_boundary;
    uint8_t  input_headroom_low;
    uint8_t  downward_blocked;
} svc_charge_cv_observation_t;

/* variables ----------------------------------------------------------------*/
/* Cached voltage-ratio reference, permille. Acquired at startup; recalculated
 * on possible cached/live boundary crossing, not on every control tick.
 * CC keeps its reference guard. MPPT cancels downward crossings, retains
 * applied D after ratio drift and permits upward recovery below reference.
 * CV downward steps may reach reference minus its trial margin;
 * a step below that floor holds applied duty/PWM, not a forced clamp.
 * Fault retains value; stop/restart clears to zero.
 * Trial ideal boundary, not measured zero current. 1..950 is encoding range
 * only; zero output uses PWM shutdown. No fixed 15% charging lower bound.
 */
extern uint16_t g_charge_duty_min_reference_permille;
/* Retained until a new valid CV frame or explicit init/start/stop reset. */
extern svc_charge_cv_observation_t g_charge_cv_observation;
/* function  ----------------------------------------------------------------*/
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
platform_err_t svc_charge_set_temperature_config(
    const svc_charge_temperature_config_t *p_config);
platform_err_t
svc_charge_get_temperature_config(svc_charge_temperature_config_t *p_config);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_H */
/* end of file --------------------------------------------------------------*/
