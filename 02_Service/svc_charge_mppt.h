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

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_CHARGE_MPPT_PHASE_T
{
    SVC_CHARGE_MPPT_PHASE_OFF = 0U,
    SVC_CHARGE_MPPT_PHASE_STARTUP,
    SVC_CHARGE_MPPT_PHASE_TRACKING,
    SVC_CHARGE_MPPT_PHASE_PAUSED,
    SVC_CHARGE_MPPT_PHASE_REACQUIRE,
} svc_charge_mppt_phase_t;

typedef struct SVC_CHARGE_MPPT_RESULT_T
{
    uint16_t duty_permille;
    uint8_t  duty_changed;
    uint8_t  duty_high_limited;
} svc_charge_mppt_result_t;

/* variables ----------------------------------------------------------------*/
/* function  ----------------------------------------------------------------*/
void                    svc_charge_mppt_reset(void);
svc_charge_mppt_phase_t svc_charge_mppt_get_phase(void);
/* Arm one bounded coarse search after normal startup; main context only.
 * CC/CV arbitration and protection remain the caller's responsibility. */
void svc_charge_mppt_startup_begin(void);
/* Discard interrupted power comparison without resetting direction/debug data.
 * Main context only; caller supplies the current applied duty. */
void svc_charge_mppt_pause(uint16_t current_duty_permille);
/* Rebuild baseline and verify a fresh fine-step direction at applied duty. */
void           svc_charge_mppt_resume(uint16_t current_duty_permille);
platform_err_t svc_charge_mppt_update(const svc_measure_value_t *p_value,
                                      uint16_t current_duty_permille,
                                      svc_charge_mppt_result_t *p_result);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_CHARGE_MPPT_H */
/* end of file --------------------------------------------------------------*/
