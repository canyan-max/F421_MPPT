/**
 ******************************************************************************
 *@file               :   svc_discharge_protection.h
 *@brief              :   Evaluate discharge faults and all protection timers.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_DISCHARGE_PROTECTION_H
#define SVC_DISCHARGE_PROTECTION_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "svc_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_DISCHARGE_FAULT_T
{
    SVC_DISCHARGE_FAULT_NONE             = 0U,
    SVC_DISCHARGE_FAULT_TEMPERATURE_LOW  = 1U,
    SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH = 2U,
    SVC_DISCHARGE_FAULT_BATTERY_LOW      = 4U,
    SVC_DISCHARGE_FAULT_OVERCURRENT      = 8U,
    SVC_DISCHARGE_FAULT_MEASUREMENT      = 16U,
    SVC_DISCHARGE_FAULT_GPIO             = 32U,
    SVC_DISCHARGE_FAULT_CONFIG           = 64U,
    SVC_DISCHARGE_FAULT_SHORT_CIRCUIT    = 128U,
} svc_discharge_fault_t;

/* variables ----------------------------------------------------------------*/
/* Main context only. PB0 ISR latch remains owned by the BSP. */
extern uint32_t g_discharge_fault_flags;

/* function  ----------------------------------------------------------------*/
/* Resets service timers/flags, never clears the BSP hardware-event latch. */
void svc_discharge_protection_reset(void);
void svc_discharge_protection_latch_fault(svc_discharge_fault_t fault);
/* Once per task slot, including bad/stale samples. Nonzero invalidates the
 * caller's cached output command, even if the holdoff expires this cycle. */
uint8_t svc_discharge_protection_update_short(void);
/* OK + fresh=0 means no new sample yet, not a successful recovery.
 * Invalid acquisition/NTC or stale timeout latches MEASUREMENT. */
platform_err_t
svc_discharge_protection_check_sample(platform_err_t             status,
                                      const svc_measure_value_t *p_value,
                                      uint8_t                   *p_fresh);
/* Fresh valid samples only. Returns pending trip conditions; main may keep
 * an already-ON load running during confirmation, never start into them. */
uint8_t svc_discharge_protection_update(const svc_measure_value_t *p_value,
                                        uint8_t output_off_confirmed);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_DISCHARGE_PROTECTION_H */
/* end of file --------------------------------------------------------------*/
