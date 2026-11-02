/**
 ******************************************************************************
 *@file               :   at32f421_uart_adapter.c
 *@brief              :   Adapt logical UART IDs to independent byte transports.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "at32f421_wk_config.h"
#include "plat_uart.h"
#include "plat_sys.h"
#include "board_resources.h"
/* define   -----------------------------------------------------------------*/
#define UART_RX_ERROR_FLAGS (USART_PERR_FLAG | USART_FERR_FLAG | \
                             USART_NERR_FLAG | USART_ROERR_FLAG)

/* typedef ------------------------------------------------------------------*/
typedef enum UART_TX_STATE_T
{
    UART_TX_IDLE = 0U,
    UART_TX_LOADING,
    UART_TX_WAIT_COMPLETE,
} uart_tx_state_t;

typedef struct
{
    plat_uart_callback_t     pf_callback;
    const uint8_t           *p_tx_data;
    uint16_t                 count;
    uint16_t                 position;
    volatile uart_tx_state_t tx_state;
} uart_context_t;

/* variables ----------------------------------------------------------------*/
/* Board logical IDs select hardware; each UART has a separate owner/state.
 * Adding another supported resource never needs another adapter file. */
static usart_type *const p_uart_bindings[BOARD_UART_RESOURCE_NUM] = {USART1,
                                                                     USART2};
static uart_context_t    g_uart_contexts[BOARD_UART_RESOURCE_NUM];

/* private  functions  ------------------------------------------------------*/
static void disable_tx_interrupts(usart_type *p_uart)
{
    usart_interrupt_enable(p_uart, USART_TDBE_INT, FALSE);
    usart_interrupt_enable(p_uart, USART_TDC_INT, FALSE);
}

static uint32_t map_rx_errors(uint32_t status)
{
    uint32_t errors = 0U;

    if(0U != (status & USART_PERR_FLAG))
    {
        errors |= PLAT_UART_ERROR_PARITY;
    }
    if(0U != (status & USART_FERR_FLAG))
    {
        errors |= PLAT_UART_ERROR_FRAME;
    }
    if(0U != (status & USART_NERR_FLAG))
    {
        errors |= PLAT_UART_ERROR_NOISE;
    }
    if(0U != (status & USART_ROERR_FLAG))
    {
        errors |= PLAT_UART_ERROR_OVERRUN;
    }
    return errors;
}

/* exported functions -------------------------------------------------------*/
platform_err_t plat_uart_start(plat_uart_id_t       id,
                               plat_uart_callback_t pf_callback)
{
    uint32_t        mask;
    usart_type     *p_uart;
    uart_context_t *p_context;

    if(id >= BOARD_UART_RESOURCE_NUM || NULL == pf_callback)
    {
        return PLATFORM_ERR_PARAM;
    }
    p_uart    = p_uart_bindings[id];
    p_context = &g_uart_contexts[id];
    mask      = plat_irq_save();
    if(NULL != p_context->pf_callback)
    {
        plat_irq_restore(mask);
        return PLATFORM_ERR_BUSY;
    }
    disable_tx_interrupts(p_uart);
    usart_interrupt_enable(p_uart, USART_RDBF_INT, FALSE);
    usart_interrupt_enable(p_uart, USART_ERR_INT, FALSE);
    usart_flag_clear(p_uart, UART_RX_ERROR_FLAGS);
    usart_flag_clear(p_uart, USART_TDC_FLAG);
    p_context->p_tx_data   = NULL;
    p_context->tx_state    = UART_TX_IDLE;
    p_context->pf_callback = pf_callback;
    usart_interrupt_enable(p_uart, USART_ERR_INT, TRUE);
    usart_interrupt_enable(p_uart, USART_RDBF_INT, TRUE);
    plat_irq_restore(mask);
    return PLATFORM_ERR_OK;
}

platform_err_t
plat_uart_send_async(plat_uart_id_t id, const uint8_t *p_data, uint16_t count)
{
    uint32_t        mask;
    usart_type     *p_uart;
    uart_context_t *p_context;

    if(id >= BOARD_UART_RESOURCE_NUM || NULL == p_data || 0U == count)
    {
        return PLATFORM_ERR_PARAM;
    }
    p_uart    = p_uart_bindings[id];
    p_context = &g_uart_contexts[id];
    mask      = plat_irq_save();
    if(NULL == p_context->pf_callback)
    {
        plat_irq_restore(mask);
        return PLATFORM_ERR_HW;
    }
    if(UART_TX_IDLE != p_context->tx_state)
    {
        plat_irq_restore(mask);
        return PLATFORM_ERR_BUSY;
    }
    disable_tx_interrupts(p_uart);
    usart_flag_clear(p_uart, USART_TDC_FLAG);
    p_context->p_tx_data = p_data;
    p_context->count     = count;
    p_context->position  = 0U;
    p_context->tx_state  = UART_TX_LOADING;
    usart_interrupt_enable(p_uart, USART_TDBE_INT, TRUE);
    plat_irq_restore(mask);
    return PLATFORM_ERR_OK;
}

uint8_t plat_uart_tx_is_busy(plat_uart_id_t id)
{
    return id < BOARD_UART_RESOURCE_NUM &&
           UART_TX_IDLE != g_uart_contexts[id].tx_state;
}

platform_err_t plat_uart_abort_tx(plat_uart_id_t id)
{
    uint32_t        mask;
    usart_type     *p_uart;
    uart_context_t *p_context;

    if(id >= BOARD_UART_RESOURCE_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    p_uart    = p_uart_bindings[id];
    p_context = &g_uart_contexts[id];
    mask      = plat_irq_save();
    if(NULL == p_context->pf_callback)
    {
        plat_irq_restore(mask);
        return PLATFORM_ERR_HW;
    }
    disable_tx_interrupts(p_uart);
    p_context->tx_state  = UART_TX_IDLE;
    p_context->p_tx_data = NULL;
    plat_irq_restore(mask);
    return PLATFORM_ERR_OK;
}

void plat_uart_irq_handler(plat_uart_id_t id)
{
    uint32_t        status;
    uint16_t        data;
    usart_type     *p_uart;
    uart_context_t *p_context;

    if(id >= BOARD_UART_RESOURCE_NUM)
    {
        return;
    }
    /* Exactly one status/data sequence: clearing errors also consumes RX. */
    p_uart    = p_uart_bindings[id];
    p_context = &g_uart_contexts[id];
    status    = p_uart->sts;
    if(0U != (status & (UART_RX_ERROR_FLAGS | USART_RDBF_FLAG)))
    {
        data = usart_data_receive(p_uart);
        if(NULL != p_context->pf_callback)
        {
            if(0U != (status & UART_RX_ERROR_FLAGS))
            {
                p_context->pf_callback(id, PLAT_UART_EVENT_RX_ERROR,
                                       map_rx_errors(status));
            }
            else
            {
                p_context->pf_callback(id, PLAT_UART_EVENT_RX_BYTE,
                                       data & 0xFFU);
            }
        }
    }
    if(NULL == p_context->pf_callback)
    {
        disable_tx_interrupts(p_uart);
        usart_interrupt_enable(p_uart, USART_RDBF_INT, FALSE);
        usart_interrupt_enable(p_uart, USART_ERR_INT, FALSE);
        return;
    }
    /* Test completion before loading: an old TDC snapshot must never finish
     * the byte loaded by the TDBE branch in this invocation. */
    if(UART_TX_WAIT_COMPLETE == p_context->tx_state &&
       RESET != usart_interrupt_flag_get(p_uart, USART_TDC_FLAG))
    {
        disable_tx_interrupts(p_uart);
        usart_flag_clear(p_uart, USART_TDC_FLAG);
        p_context->p_tx_data = NULL;
        p_context->tx_state  = UART_TX_IDLE;
        p_context->pf_callback(id, PLAT_UART_EVENT_TX_DONE, 0U);
    }
    if(UART_TX_LOADING == p_context->tx_state &&
       RESET != usart_interrupt_flag_get(p_uart, USART_TDBE_FLAG))
    {
        usart_data_transmit(p_uart, p_context->p_tx_data[p_context->position]);
        p_context->position++;
        if(p_context->position == p_context->count)
        {
            usart_interrupt_enable(p_uart, USART_TDBE_INT, FALSE);
            p_context->tx_state = UART_TX_WAIT_COMPLETE;
            usart_interrupt_enable(p_uart, USART_TDC_INT, TRUE);
        }
    }
}

/* end of file --------------------------------------------------------------*/
