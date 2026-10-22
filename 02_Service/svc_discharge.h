/**
 ******************************************************************************
 *@file               :   svc_discharge.h
 *@brief              :   Define independent protected load-switch control.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_DISCHARGE_H
#define SVC_DISCHARGE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "svc_discharge_config.h"
#include "svc_discharge_protection.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum SVC_DISCHARGE_STATE_T
{
    SVC_DISCHARGE_STATE_WAIT_SAMPLE = 0U,
    SVC_DISCHARGE_STATE_MANUAL_OFF,
    SVC_DISCHARGE_STATE_ON,
    SVC_DISCHARGE_STATE_PROTECTED,
    SVC_DISCHARGE_STATE_FAULT,
} svc_discharge_state_t;

/* variables ----------------------------------------------------------------*/
/* Main context only. Default permission is 1; faults always override it. */
extern volatile uint8_t g_discharge_manual_enabled;
/* Last successful GPIO command, not electrical feedback from the MOSFET. */
extern uint8_t               g_discharge_output_enabled;
extern svc_discharge_state_t g_discharge_state;

/* function  ----------------------------------------------------------------*/
/* After GPIO/EXINT init: PB11 off, separate 12/24 V profile, manual default 1.
 * Reinitialization never clears the BSP short-circuit latch. */
platform_err_t svc_discharge_init(svc_discharge_battery_system_t system);
/* Main context, exactly once per period; never depends on charge or Vin. */
void svc_discharge_process(void);
/* Communications may request permission, but may not bypass protection. */
platform_err_t                svc_discharge_set_manual_enabled(uint8_t enabled);
const svc_discharge_config_t *svc_discharge_get_config(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_DISCHARGE_H */
/* end of file --------------------------------------------------------------*/
