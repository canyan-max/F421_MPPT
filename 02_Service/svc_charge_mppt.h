/**
 ******************************************************************************
 *@file               :   svc_charge_mppt.h
 *@brief              :   Define direct duty power-perturbation verification.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_CHARGE_MPPT_H
#define SVC_CHARGE_MPPT_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "svc_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SVC_CHARGE_MPPT_TRACE_NUM (32U)

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_MPPT_TRACE_FREEZE_T
{
    SVC_CHARGE_MPPT_TRACE_RUNNING = 0U,
    SVC_CHARGE_MPPT_TRACE_CV,
    SVC_CHARGE_MPPT_TRACE_FAULT,
    SVC_CHARGE_MPPT_TRACE_STOP,
} svc_charge_mppt_trace_freeze_t;

typedef struct SVC_CHARGE_MPPT_TRACE_T
{
    uint32_t sequence;     /* Across tracker resets; wraps at UINT32_MAX. */
    uint32_t service_tick; /* Same 10 ms clock as g_charge_trace. */
    uint32_t measure_sequence;
    uint32_t compare_count;
    uint32_t power_before_uw; /* Actual averaged comparison windows. */
    uint32_t power_after_uw;
    uint32_t input_voltage_end_mv; /* Window-end sample, not an average. */
    uint32_t battery_voltage_end_mv;
    uint32_t input_current_end_ma; /* Buck output bus current. */
    uint16_t duty_before_permille; /* D used for the preceding window. */
    uint16_t duty_after_permille;  /* Applied D used for this window. */
    uint16_t next_duty_requested_permille; /* Proposal, not PWM confirmation. */
    uint8_t  perturb_increase;
    uint8_t  next_direction_increase;
    int8_t   power_change;
    uint8_t  valid;
} svc_charge_mppt_trace_t;

typedef struct SVC_CHARGE_MPPT_RESULT_T
{
    uint16_t duty_permille;
    uint8_t  duty_changed;
    uint8_t  duty_high_limited;
} svc_charge_mppt_result_t;

/* variables ----------------------------------------------------------------*/
extern volatile svc_charge_mppt_trace_t
                         g_charge_mppt_trace[SVC_CHARGE_MPPT_TRACE_NUM];
extern volatile uint16_t g_charge_mppt_trace_write_index;
extern volatile uint16_t g_charge_mppt_trace_count;
extern volatile svc_charge_mppt_trace_freeze_t g_charge_mppt_trace_freeze;

/* function  ----------------------------------------------------------------*/
/* Main context only. Diagnostic history survives tracker reset/pause. */
void svc_charge_mppt_trace_clear(void);
void svc_charge_mppt_trace_freeze(svc_charge_mppt_trace_freeze_t reason);
void svc_charge_mppt_trace_set_context(uint32_t service_tick,
                                       uint32_t measure_sequence);
void svc_charge_mppt_reset(void);
/* Main-context diagnostic access; does not advance tracking. */
uint32_t svc_charge_mppt_get_compare_count(void);
/* Discard interrupted power comparison without resetting direction/debug data.
 * Main context only; caller supplies the current applied duty. */
void           svc_charge_mppt_pause(uint16_t current_duty_permille);
platform_err_t svc_charge_mppt_update(const svc_measure_value_t *p_value,
                                      uint16_t current_duty_permille,
                                      svc_charge_mppt_result_t *p_result);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_MPPT_H */
/* end of file --------------------------------------------------------------*/
