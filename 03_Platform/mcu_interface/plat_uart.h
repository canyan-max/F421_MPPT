/**
 ******************************************************************************
 *@file               :   plat_uart.h
 *@brief              :   Provide interrupt driven UART byte transport.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PLAT_UART_H
#define PLAT_UART_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "plat_resource.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define PLAT_UART_ERROR_PARITY (1U)
#define PLAT_UART_ERROR_FRAME (2U)
#define PLAT_UART_ERROR_NOISE (4U)
#define PLAT_UART_ERROR_OVERRUN (8U)

/* typedef ------------------------------------------------------------------*/
typedef enum PLAT_UART_EVENT_T
{
    PLAT_UART_EVENT_RX_BYTE = 0U,
    PLAT_UART_EVENT_RX_ERROR,
    PLAT_UART_EVENT_TX_DONE,
} plat_uart_event_t;

/* Synchronous ISR callback: value is a byte, error mask, or zero for TX_DONE.
 * No waits, logging, parsing, or reentrant start/send/abort calls. */
typedef void (*plat_uart_callback_t)(plat_uart_id_t    id,
                                     plat_uart_event_t event,
                                     uint32_t          value);

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context only. Call once after generated UART init, which must leave
 * UART interrupts disabled until this binding. Reuses baud/pins/clock config.
 * Each logical ID exclusively owns its UART and independent transfer state.
 * Discards pre-start RX status/data, then enables RX and error interrupts. */
platform_err_t plat_uart_start(plat_uart_id_t       id,
                               plat_uart_callback_t pf_callback);
/* Main context only; one outstanding transfer. Borrows count bytes until
 * TX_DONE or abort returns. BUSY leaves the active transfer untouched. */
platform_err_t
plat_uart_send_async(plat_uart_id_t id, const uint8_t *p_data, uint16_t count);
uint8_t plat_uart_tx_is_busy(plat_uart_id_t id);
/* Main context only. Stops further loading and releases the borrowed buffer.
 * Already loaded bytes may still leave the wire. No TX_DONE is generated. */
platform_err_t plat_uart_abort_tx(plat_uart_id_t id);
/* The generated IRQ must delegate exclusively here; no second data read. */
void plat_uart_irq_handler(plat_uart_id_t id);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PLAT_UART_H */
/* end of file --------------------------------------------------------------*/
