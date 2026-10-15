/**
 ******************************************************************************
 *@file               :   svc_measure.h
 *@brief              :   Provide simple measurement access to applications.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_MEASURE_H
#define SVC_MEASURE_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "bsp_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define SVC_MEASURE_TEMPERATURE_INVALID (BSP_MEASURE_TEMPERATURE_INVALID)
#define SVC_MEASURE_READY_FRAME_COUNT (128U)

/* typedef ------------------------------------------------------------------*/
/* IN_CUR is Buck output current; OUT_CUR is external load current. */
typedef bsp_measure_value_t svc_measure_value_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Startup once after ADC/DMA initialization, before the sampling timer starts.
 * Main context only. Does not start the trigger or enable power output. */
platform_err_t svc_measure_init(void);
/* Nonblocking processing; call once in each main-loop iteration. */
void svc_measure_process(void);
/* Copies the latest value. Returns BUSY before the first valid sample. */
platform_err_t svc_measure_get(svc_measure_value_t *p_value);
/* Readiness requires consecutive valid frames with no sequence gap. */
uint8_t svc_measure_is_ready(void);
/* Latest converted frame sequence; used to detect stale data. */
uint32_t svc_measure_get_sequence(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_MEASURE_H */
/* end of file --------------------------------------------------------------*/
