#ifndef PLAT_ADC_H
#define PLAT_ADC_H
#include <stdint.h>
#include "plat_error.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PLAT_ADC_CHANNEL_COUNT 5U
/* Sequence indices, not physical ADC channel numbers. Raw 12-bit counts. */
typedef enum {
    PLAT_ADC_OUT_CUR = 0, PLAT_ADC_IN_CUR, PLAT_ADC_BAT_VOL,
    PLAT_ADC_VIN_VOL, PLAT_ADC_NTC
} plat_adc_sample_index_t;
typedef enum {
    PLAT_ADC_EVENT_HALF, PLAT_ADC_EVENT_FULL, PLAT_ADC_EVENT_ERROR
} plat_adc_event_t;
/* ISR callback: no blocking/logging. samples is NULL on error.
 * Buffer belongs to caller, must remain alive until stop. DMA writes it.
 * Completed half is valid only until DMA wraps to that half (100 us for
 * 100 elements at 100 kHz). Do not defer a pointer without deadline/overrun
 * handling. Both flags pending are treated as overrun and stop acquisition. */
typedef void (*plat_adc_callback_t)(plat_adc_event_t event,
    const volatile uint16_t *samples, uint16_t count);
/* Main context only; never call start/stop from callbacks or other ISRs.
 * Startup only: ADC calibrated, DMA initialized, TMR trigger stopped and no
 * conversion in progress. Buffer halfword aligned; length multiple of 10.
 * Does not initialize ADC, start timer, or enable power PWM. */
platform_err_t plat_adc_start(volatile uint16_t *buffer, uint16_t count,
    plat_adc_callback_t callback);
/* Stop trigger first. Stops DMA only; leaves ADC and timer configuration. */
void plat_adc_stop(void);
/* Invoke once at entry to DMA1_Channel1_IRQHandler. */
void plat_adc_dma_irq_handler(void);
#ifdef __cplusplus
}
#endif
#endif
