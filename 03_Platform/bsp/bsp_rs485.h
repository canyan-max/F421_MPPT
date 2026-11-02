/**
 ******************************************************************************
 *@file               :   bsp_rs485.h
 *@brief              :   Buffer RS485 receive segments and control direction.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_RS485_H
#define BSP_RS485_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define BSP_RS485_BUFFER_SIZE (256U)
/* Trial parameters for generated 9600/8N1; not Modbus compliance claims.
 * Five 1 ms tick counts give roughly 4..5 ms receive silence. */
#define BSP_RS485_RX_GAP_MS (5U)
/* One tick difference: about 0..1 ms plus main-loop scheduling latency.
 * Trial setting; not a guaranteed minimum 1 ms hardware setup time. */
#define BSP_RS485_TX_SETUP_MS (1U)

/* typedef ------------------------------------------------------------------*/
typedef enum BSP_RS485_RX_RESULT_T
{
    BSP_RS485_RX_OK = 0U,
    BSP_RS485_RX_EMPTY,
    BSP_RS485_RX_DROPPED,
    BSP_RS485_RX_BUFFER_SMALL,
    BSP_RS485_RX_PARAM,
    BSP_RS485_RX_ERROR,
} bsp_rs485_rx_result_t;

typedef enum BSP_RS485_TX_RESULT_T
{
    BSP_RS485_TX_IDLE = 0U,
    BSP_RS485_TX_BUSY,
    BSP_RS485_TX_DONE,
    BSP_RS485_TX_FAILED,
} bsp_rs485_tx_result_t;

typedef struct BSP_RS485_STATS_T
{
    uint32_t rx_bytes;
    uint32_t rx_segments;
    uint32_t rx_errors;
    uint32_t rx_overflows;
    uint32_t discarded_segments;
    uint32_t tx_segments;
    uint32_t tx_errors;
    uint32_t tx_timeouts;
} bsp_rs485_stats_t;

/* variables ----------------------------------------------------------------*/
/* Watch diagnostics; multi-field reads are not an atomic snapshot. */
extern volatile bsp_rs485_stats_t g_rs485_stats;

/* function  ----------------------------------------------------------------*/
/* Main only, once after GPIO/UART generated init. PF7 high=TX, low=RX,
 * DE and /RE tied together (user confirmed). Starts in receive mode. */
platform_err_t bsp_rs485_init(void);
/* Call once per SysTick after advancing the existing 1 ms timebase.
 * Bounded work only; publishes FIFO endpoints, never copies segments. */
void bsp_rs485_tick_1ms(void);
/* Main only, bounded and nonblocking; advances TX setup/timeout recovery. */
void bsp_rs485_process(void);
/* Main only, single consumer. OK returns one full segment. EMPTY is normal.
 * DROPPED discards one damaged segment, count=0. BUFFER_SMALL preserves the
 * segment and returns its required count. PARAM leaves outputs untouched;
 * ERROR means not initialized or unable to continue (reset required).
 * No protocol parsing or per-error notifications. */
bsp_rs485_rx_result_t
bsp_rs485_read_segment(uint8_t *p_data, uint16_t capacity, uint16_t *p_count);
/* Main only. One outstanding send, borrowing 1..BUFFER_SIZE bytes until the
 * result leaves BUSY. PARAM/BUSY do not replace the previous result.
 * DONE means TDC plus successful switch to RX; FAILED never means success.
 * Timeout returns to RX; allow a fresh silence interval before another send.
 * GPIO/adapter failure stops further communication until reset. */
platform_err_t bsp_rs485_send_async(const uint8_t *p_data, uint16_t count);
bsp_rs485_tx_result_t bsp_rs485_tx_get_result(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* BSP_RS485_H */
/* end of file --------------------------------------------------------------*/
