/**
 ******************************************************************************
 *@file               :   plat_adc.h
 *@brief              :   Provide raw ADC scan data and DMA event interfaces.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PLAT_ADC_H
#define PLAT_ADC_H

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "plat_resource.h"
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef enum PLAT_ADC_EVENT_T
{
    PLAT_ADC_EVENT_HALF = 0U,
    PLAT_ADC_EVENT_FULL,
    PLAT_ADC_EVENT_ERROR,
    PLAT_ADC_EVENT_OVERRUN,
} plat_adc_event_t;
/* ISR callback: no blocking/logging. NULL samples on error or overrun.
 * Buffer belongs to caller, must remain alive until stop. DMA writes it.
 * Each completed half contains one configured ADC scan and is valid until
 * DMA returns to it. Do not defer a pointer without deadline/overrun
 * handling. Both flags pending discard this batch and report overrun;
 *
 * acquisition continues. A transfer error stops DMA. */
typedef void (*plat_adc_callback_t)(plat_adc_id_t            id,
                                    plat_adc_event_t         event,
                                    const volatile uint16_t *p_samples,
                                    uint16_t                 count);

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only; never call start/stop from callbacks or other ISRs.
 * Startup only: ADC calibrated, DMA initialized, TMR trigger stopped and no
 * conversion in progress. Buffer halfword aligned; length must equal twice
 * the configured ordinary scan length (one complete scan per half).
 * Does not initialize ADC, start timer, or enable power PWM.
 * Each ID exclusively owns its ADC and DMA channel; IDs do not schedule
 * competing groups on one ADC. Invalid IDs are rejected. */
platform_err_t plat_adc_start(plat_adc_id_t       id,
                              volatile uint16_t  *p_buffer,
                              uint16_t            count,
                              plat_adc_callback_t pf_callback);
/* Stop trigger first. Stops DMA only; leaves ADC and timer configuration. */
platform_err_t plat_adc_stop(plat_adc_id_t id);
/* Dispatch the bound group ID from its DMA IRQ. Invalid IDs are ignored. */
void plat_adc_dma_irq_handler(plat_adc_id_t id);
/* Order shared sample accesses across ISR/main publication. */
void plat_adc_memory_barrier(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PLAT_ADC_H */
/* end of file --------------------------------------------------------------*/

