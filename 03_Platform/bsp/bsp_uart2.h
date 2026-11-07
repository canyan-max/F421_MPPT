/**
 ***************************************************************************************************
 *@file               :   bsp_uart2.h
 *@brief              :   Buffer ordinary UART2 receive segments and transmit.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef BSP_UART2_H
#define BSP_UART2_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/
#define BSP_UART2_BUFFER_SIZE (256U)
/* Trial parameters for generated 9600/8N1; not Modbus compliance claims.
 * Five 1 ms tick counts give roughly 4..5 ms receive silence. */
#define BSP_UART2_RX_GAP_MS (5U)
/* typedef ---------------------------------------------------------------------------------------*/
typedef enum BSP_UART2_RX_RESULT_T {
    BSP_UART2_RX_OK = 0U,
    BSP_UART2_RX_EMPTY,
    BSP_UART2_RX_DROPPED,
    BSP_UART2_RX_BUFFER_SMALL,
    BSP_UART2_RX_PARAM,
    BSP_UART2_RX_ERROR,
} bsp_uart2_rx_result_t;

typedef enum BSP_UART2_TX_RESULT_T {
    BSP_UART2_TX_IDLE = 0U,
    BSP_UART2_TX_BUSY,
    BSP_UART2_TX_DONE,
    BSP_UART2_TX_FAILED,
} bsp_uart2_tx_result_t;

typedef struct BSP_UART2_STATS_T {
    uint32_t rx_bytes;
    uint32_t rx_segments;
    uint32_t rx_errors;
    uint32_t rx_overflows;
    uint32_t discarded_segments;
    uint32_t tx_segments;
    uint32_t tx_errors;
    uint32_t tx_timeouts;
} bsp_uart2_stats_t;

/* variables -------------------------------------------------------------------------------------*/
/* Watch diagnostics; multi-field reads are not an atomic snapshot. */
extern volatile bsp_uart2_stats_t g_uart2_stats;

/* function  -------------------------------------------------------------------------------------*/
/* Main only, once after generated UART2 init. No direction GPIO or setup wait.
 * Reuses 9600/8N1. RX remains enabled during transmission. */
platform_err_t bsp_uart2_init(void);
/* Call once per SysTick after advancing the existing 1 ms timebase.
 * Bounded work only; publishes FIFO endpoints, never copies segments. */
void bsp_uart2_tick_1ms(void);
/* Main only, bounded and nonblocking; handles bounded TX timeout recovery. */
void bsp_uart2_process(void);
/* Main only, single consumer. OK returns one full segment. EMPTY is normal.
 * DROPPED discards one damaged segment, count=0. BUFFER_SMALL preserves the
 * segment and returns its required count. PARAM leaves outputs untouched;
 * ERROR means not initialized or unable to continue (reset required).
 * No protocol parsing or per-error notifications. */
bsp_uart2_rx_result_t bsp_uart2_read_segment(uint8_t *p_data, uint16_t capacity, uint16_t *p_count);
/* Main only. One outstanding send, borrowing 1..BUFFER_SIZE bytes until the
 * result leaves BUSY. PARAM/BUSY do not replace the previous result.
 * DONE means TDC; FAILED never means success.
 * After timeout abort succeeds, wait a fresh interval before another send.
 * Adapter failure stops further communication until reset. */
platform_err_t        bsp_uart2_send_async(const uint8_t *p_data, uint16_t count);
bsp_uart2_tx_result_t bsp_uart2_tx_get_result(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_UART2_H */
/* end of file -----------------------------------------------------------------------------------*/
