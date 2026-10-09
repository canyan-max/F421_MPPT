#include <stddef.h>
#include "at32f421_wk_config.h"
#include "plat_adc.h"
static volatile uint16_t *adc_buffer;
static uint16_t adc_count;
static plat_adc_callback_t adc_callback;
static volatile uint8_t adc_running;
static void clear_flags(void)
{
    dma_flag_clear(DMA1_FDT1_FLAG);
    dma_flag_clear(DMA1_HDT1_FLAG);
    dma_flag_clear(DMA1_DTERR1_FLAG);
}
platform_err_t plat_adc_start(volatile uint16_t *buffer, uint16_t count,
    plat_adc_callback_t callback)
{
    if(buffer == NULL || callback == NULL || count == 0U ||
       count % (2U * PLAT_ADC_CHANNEL_COUNT) != 0U ||
       ((uint32_t)buffer & 1U) != 0U) return PLATFORM_ERR_PARAM;
    if(adc_running != 0U) return PLATFORM_ERR_BUSY;
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    clear_flags();
    adc_buffer = buffer;
    adc_count = count;
    adc_callback = callback;
    DMA1_CHANNEL1->paddr = (uint32_t)&ADC1->odt;
    DMA1_CHANNEL1->maddr = (uint32_t)buffer;
    DMA1_CHANNEL1->dtcnt = count;
    adc_running = 1U;
    __DMB();
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
    return PLATFORM_ERR_OK;
}
void plat_adc_stop(void)
{
    /* Main context only; prevent ISR dispatch before releasing the buffer. */
    adc_running = 0U;
    __DMB();
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    clear_flags();
    adc_callback = NULL;
    adc_buffer = NULL;
    adc_count = 0U;
}
void plat_adc_dma_irq_handler(void)
{
    uint8_t half = dma_flag_get(DMA1_HDT1_FLAG) != RESET;
    uint8_t full = dma_flag_get(DMA1_FDT1_FLAG) != RESET;
    uint8_t error = dma_flag_get(DMA1_DTERR1_FLAG) != RESET;
    plat_adc_callback_t callback = adc_callback;
    clear_flags();
    if(adc_running == 0U || callback == NULL) return;
    if(error != 0U || (half != 0U && full != 0U)) {
        dma_channel_enable(DMA1_CHANNEL1, FALSE);
        adc_running = 0U;
        callback(PLAT_ADC_EVENT_ERROR, NULL, 0U);
        return;
    }
    __DMB();
    if(half != 0U) callback(PLAT_ADC_EVENT_HALF, adc_buffer, adc_count / 2U);
    if(full != 0U) callback(PLAT_ADC_EVENT_FULL, adc_buffer + adc_count / 2U, adc_count / 2U);
}
