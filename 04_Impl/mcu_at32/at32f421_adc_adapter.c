/**
 ******************************************************************************
 *@file               :   at32f421_adc_adapter.c
 *@brief              :   Adapt ADC DMA buffer binding and transfer events for
 *                        AT32F421.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <stddef.h>
#include "at32f421_wk_config.h"
#include "plat_adc.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct ADC_BINDING_T
{
    adc_type         *p_adc;
    dma_channel_type *p_dma;
    uint32_t          half_flag;
    uint32_t          full_flag;
    uint32_t          error_flag;
} adc_binding_t;
typedef struct ADC_STATE_T
{
    volatile uint16_t  *p_buffer;
    uint16_t            count;
    plat_adc_callback_t pf_callback;
    volatile uint8_t    running;
} adc_state_t;
/* variables ----------------------------------------------------------------*/
/* Generated init owns scan order, scan length and triggering. */
static const adc_binding_t g_adc_bindings[] = {
    {ADC1, DMA1_CHANNEL1, DMA1_HDT1_FLAG, DMA1_FDT1_FLAG, DMA1_DTERR1_FLAG},
};
static adc_state_t
    g_adc_states[sizeof(g_adc_bindings) / sizeof(g_adc_bindings[0U])];
/* private  functions  ------------------------------------------------------*/
static void clear_flags(const adc_binding_t *p_binding)
{
    dma_flag_clear(p_binding->full_flag);
    dma_flag_clear(p_binding->half_flag);
    dma_flag_clear(p_binding->error_flag);
}
/* exported functions -------------------------------------------------------*/
platform_err_t plat_adc_start(plat_adc_id_t       id,
                              volatile uint16_t  *p_buffer,
                              uint16_t            count,
                              plat_adc_callback_t pf_callback)
{
    const adc_binding_t *p_binding = NULL;
    adc_state_t         *p_state   = NULL;
    uint16_t channel_count;
    if(id >= sizeof(g_adc_bindings) / sizeof(g_adc_bindings[0U]))
    {
        return PLATFORM_ERR_PARAM;
    }
    p_binding = &g_adc_bindings[id];
    p_state   = &g_adc_states[id];
    channel_count = (uint16_t)(p_binding->p_adc->osq1_bit.oclen + 1U);

    if(NULL == p_buffer || NULL == pf_callback || 0U == count ||
       count != 2U * channel_count ||
       0U != ((uint32_t)p_buffer & 1U))
    {
        return PLATFORM_ERR_PARAM;
    }
    if(0U != p_state->running)
    {
        return PLATFORM_ERR_BUSY;
    }
    dma_channel_enable(p_binding->p_dma, FALSE);
    clear_flags(p_binding);
    p_state->p_buffer       = p_buffer;
    p_state->count          = count;
    p_state->pf_callback    = pf_callback;
    p_binding->p_dma->paddr = (uint32_t)&p_binding->p_adc->odt;
    p_binding->p_dma->maddr = (uint32_t)p_buffer;
    p_binding->p_dma->dtcnt = count;
    p_state->running        = 1U;
    __DMB();
    dma_channel_enable(p_binding->p_dma, TRUE);
    return PLATFORM_ERR_OK;
}

platform_err_t plat_adc_stop(plat_adc_id_t id)
{
    const adc_binding_t *p_binding = NULL;
    adc_state_t         *p_state   = NULL;
    if(id >= sizeof(g_adc_bindings) / sizeof(g_adc_bindings[0U]))
    {
        return PLATFORM_ERR_PARAM;
    }
    p_binding = &g_adc_bindings[id];
    p_state   = &g_adc_states[id];

    /* Main context only; prevent ISR dispatch before releasing the buffer. */
    p_state->running = 0U;
    __DMB();
    dma_channel_enable(p_binding->p_dma, FALSE);
    clear_flags(p_binding);
    p_state->pf_callback = NULL;
    p_state->p_buffer    = NULL;
    p_state->count       = 0U;
    return PLATFORM_ERR_OK;
}

void plat_adc_dma_irq_handler(plat_adc_id_t id)
{
    uint8_t             half        = 0U;
    uint8_t             full        = 0U;
    uint8_t             error       = 0U;
    plat_adc_callback_t pf_callback = NULL;

    const adc_binding_t *p_binding = NULL;
    adc_state_t         *p_state   = NULL;
    if(id >= sizeof(g_adc_bindings) / sizeof(g_adc_bindings[0U]))
    {
        return;
    }
    p_binding = &g_adc_bindings[id];
    p_state   = &g_adc_states[id];

    half        = RESET != dma_flag_get(p_binding->half_flag);
    full        = RESET != dma_flag_get(p_binding->full_flag);
    error       = RESET != dma_flag_get(p_binding->error_flag);
    pf_callback = p_state->pf_callback;
    clear_flags(p_binding);
    if(0U == p_state->running || NULL == pf_callback)
    {
        return;
    }
    if(0U != error)
    {
        dma_channel_enable(p_binding->p_dma, FALSE);
        p_state->running = 0U;
        pf_callback(id, PLAT_ADC_EVENT_ERROR, NULL, 0U);
        return;
    }
    if(0U != half && 0U != full)
    {
        /* Discard potentially overwritten halves after a CPU halt. */
        pf_callback(id, PLAT_ADC_EVENT_OVERRUN, NULL, 0U);
        return;
    }
    __DMB();
    if(0U != half)
    {
        pf_callback(id, PLAT_ADC_EVENT_HALF, p_state->p_buffer,
                    p_state->count / 2U);
    }
    if(0U != full)
    {
        pf_callback(id, PLAT_ADC_EVENT_FULL,
                    p_state->p_buffer + p_state->count / 2U,
                    p_state->count / 2U);
    }
}

void plat_adc_memory_barrier(void)
{
    __DMB();
}
/* end of file --------------------------------------------------------------*/

