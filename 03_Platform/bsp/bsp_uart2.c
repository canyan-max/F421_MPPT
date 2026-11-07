/**
 ***************************************************************************************************
 *@file               :   bsp_uart2.c
 *@brief              :   Buffer UART2 segments and manage full duplex IO.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* includes --------------------------------------------------------------------------------------*/
#include "bsp_uart2.h"
#include "plat_uart.h"
#include "plat_sys.h"
#include "board_resources.h"
#include "kfifo.h"
/* define   --------------------------------------------------------------------------------------*/
#define UART2_SEGMENT_SLOTS (4U)
#define UART2_COPY_CHUNK (16U)
/* 9600/8N1 needs <2 ms per byte; include generous bounded trial margin. */
#define UART2_TX_MARGIN_MS (20U)

/* typedef ---------------------------------------------------------------------------------------*/
typedef struct {
    uint32_t end;
    uint8_t  damaged;
} uart2_segment_t;

/* variables -------------------------------------------------------------------------------------*/
volatile bsp_uart2_stats_t   g_uart2_stats;
static kfifo_t               g_rx_fifo;
static uint8_t               g_rx_buffer[BSP_UART2_BUFFER_SIZE];
static uart2_segment_t       g_segments[UART2_SEGMENT_SLOTS];
static volatile uint32_t     g_segment_in;
static volatile uint32_t     g_segment_out;
static uint32_t              g_segment_start;
static uint32_t              g_rx_last_ms;
static uint8_t               g_rx_active;
static uint8_t               g_rx_damaged;
static volatile uint8_t      g_ready;
static bsp_uart2_tx_result_t g_tx_result;
static volatile uint8_t      g_tx_busy;
static uint32_t              g_tx_start_ms;
static uint32_t              g_tx_timeout_ms;

/* private  functions  ---------------------------------------------------------------------------*/
/* Caller serializes UART/SysTick/main with a short saved-mask section. */
static void close_segment(void) {
    uint32_t slot;

    if (0U == g_rx_active) {
        return;
    }
    if ((uint32_t)(g_segment_in - g_segment_out) >= UART2_SEGMENT_SLOTS) {
        /* No consumer can read this unpublished tail. Roll it back rather
         * than mixing it with the next segment or overwriting older data. */
        g_rx_fifo.in = g_segment_start;
        g_uart2_stats.rx_overflows++;
        g_uart2_stats.discarded_segments++;
    } else {
        slot                     = g_segment_in % UART2_SEGMENT_SLOTS;
        g_segments[slot].end     = g_rx_fifo.in;
        g_segments[slot].damaged = g_rx_damaged;
        g_segment_in++;
        g_uart2_stats.rx_segments++;
    }
    g_segment_start = g_rx_fifo.in;
    g_rx_active     = 0U;
    g_rx_damaged    = 0U;
}

static void uart_event(plat_uart_id_t id, plat_uart_event_t event, uint32_t value) {
    uint32_t now_ms;
    uint8_t  byte;

    if (BOARD_UART_DEBUG != id || 0U == g_ready) {
        return;
    }
    if (PLAT_UART_EVENT_TX_DONE == event) {
        if (0U != g_tx_busy) {
            g_tx_busy   = 0U;
            g_tx_result = BSP_UART2_TX_DONE;
            g_uart2_stats.tx_segments++;
        }
        return;
    }
    now_ms = plat_tick_get_ms();
    /* Also split here if SysTick's lower priority delayed its publication. */
    if (0U != g_rx_active && (uint32_t)(now_ms - g_rx_last_ms) >= BSP_UART2_RX_GAP_MS) {
        close_segment();
    }
    g_rx_active  = 1U;
    g_rx_last_ms = now_ms;
    if (PLAT_UART_EVENT_RX_ERROR == event) {
        g_rx_damaged = 1U;
        g_uart2_stats.rx_errors++;
        return;
    }
    g_uart2_stats.rx_bytes++;
    if (0U != g_rx_damaged) {
        return;
    }
    byte = (uint8_t)value;
    if (1U != kfifo_put(&g_rx_fifo, &byte, 1U)) {
        g_rx_damaged = 1U;
        g_uart2_stats.rx_overflows++;
    }
}

/* exported functions ----------------------------------------------------------------------------*/
platform_err_t bsp_uart2_init(void) {
    platform_err_t status;
    uint32_t       mask;

    if (0U != g_ready) {
        return PLATFORM_ERR_BUSY;
    }
    if (0U != kfifo_init(&g_rx_fifo, g_rx_buffer, sizeof(g_rx_buffer))) {
        return PLATFORM_ERR_PARAM;
    }
    mask    = plat_irq_save();
    g_ready = 1U;
    status  = plat_uart_start(BOARD_UART_DEBUG, uart_event);
    if (PLATFORM_ERR_OK != status) {
        g_ready = 0U;
    }
    plat_irq_restore(mask);
    return status;
}

void bsp_uart2_tick_1ms(void) {
    uint32_t mask;
    uint32_t now_ms;

    if (0U == g_ready) {
        return;
    }
    /* UART can preempt SysTick. Serialize the time
     * check and endpoint publication; never copy data in this section. */
    mask   = plat_irq_save();
    now_ms = plat_tick_get_ms();
    if (0U != g_rx_active && (uint32_t)(now_ms - g_rx_last_ms) >= BSP_UART2_RX_GAP_MS) {
        close_segment();
    }
    plat_irq_restore(mask);
}

bsp_uart2_rx_result_t
bsp_uart2_read_segment(uint8_t *p_data, uint16_t capacity, uint16_t *p_count) {
    uart2_segment_t segment;
    uint32_t        count;
    uint32_t        copied = 0U;
    uint32_t        chunk;
    uint32_t        actual;
    uint32_t        mask;
    uint8_t         discard[UART2_COPY_CHUNK];

    if (NULL == p_data || NULL == p_count || 0U == capacity) {
        return BSP_UART2_RX_PARAM;
    }
    *p_count = 0U;
    mask     = plat_irq_save();
    if (0U == g_ready) {
        plat_irq_restore(mask);
        return BSP_UART2_RX_ERROR;
    }
    if (g_segment_out == g_segment_in) {
        plat_irq_restore(mask);
        return BSP_UART2_RX_EMPTY;
    }
    segment = g_segments[g_segment_out % UART2_SEGMENT_SLOTS];
    count   = segment.end - g_rx_fifo.out;
    if (count > BSP_UART2_BUFFER_SIZE || count > kfifo_len(&g_rx_fifo)) {
        g_ready = 0U;
        plat_irq_restore(mask);
        return BSP_UART2_RX_ERROR;
    }
    plat_irq_restore(mask);
    if (0U == segment.damaged && capacity < count) {
        *p_count = (uint16_t)count;
        return BSP_UART2_RX_BUFFER_SMALL;
    }
    while (copied < count) {
        chunk = count - copied;
        if (chunk > UART2_COPY_CHUNK) {
            chunk = UART2_COPY_CHUNK;
        }
        /* Existing kfifo has plain indexes. Serialize only tiny chunks,
         * without changing the user's component or masking a full frame. */
        mask   = plat_irq_save();
        actual = kfifo_get(&g_rx_fifo, 0U != segment.damaged ? discard : p_data + copied, chunk);
        if (actual != chunk) {
            g_ready = 0U;
            plat_irq_restore(mask);
            return BSP_UART2_RX_ERROR;
        }
        plat_irq_restore(mask);
        copied += actual;
    }
    mask = plat_irq_save();
    g_segment_out++;
    if (0U != segment.damaged) {
        g_uart2_stats.discarded_segments++;
    }
    plat_irq_restore(mask);
    if (0U != segment.damaged) {
        return BSP_UART2_RX_DROPPED;
    }
    *p_count = (uint16_t)count;
    return BSP_UART2_RX_OK;
}

platform_err_t bsp_uart2_send_async(const uint8_t *p_data, uint16_t count) {
    uint32_t       mask;
    platform_err_t status;

    if (NULL == p_data || 0U == count || count > BSP_UART2_BUFFER_SIZE) {
        return PLATFORM_ERR_PARAM;
    }
    mask = plat_irq_save();
    if (0U == g_ready) {
        plat_irq_restore(mask);
        return PLATFORM_ERR_HW;
    }
    if (0U != g_tx_busy || (BSP_UART2_TX_FAILED == g_tx_result &&
                            (uint32_t)(plat_tick_get_ms() - g_tx_start_ms) < BSP_UART2_RX_GAP_MS)) {
        plat_irq_restore(mask);
        return PLATFORM_ERR_BUSY;
    }
    status = plat_uart_send_async(BOARD_UART_DEBUG, p_data, count);
    if (PLATFORM_ERR_OK == status) {
        g_tx_start_ms   = plat_tick_get_ms();
        g_tx_timeout_ms = (uint32_t)count * 2U + UART2_TX_MARGIN_MS;
        g_tx_busy       = 1U;
        g_tx_result     = BSP_UART2_TX_BUSY;
    } else if (PLATFORM_ERR_BUSY != status) {
        g_uart2_stats.tx_errors++;
        g_ready = 0U;
    }
    plat_irq_restore(mask);
    return status;
}

bsp_uart2_tx_result_t bsp_uart2_tx_get_result(void) {
    uint32_t              mask   = plat_irq_save();
    bsp_uart2_tx_result_t result = 0U != g_tx_busy ? BSP_UART2_TX_BUSY : g_tx_result;

    plat_irq_restore(mask);
    return result;
}

void bsp_uart2_process(void) {
    uint32_t mask;

    mask = plat_irq_save();
    if (0U != g_ready && 0U != g_tx_busy &&
        (uint32_t)(plat_tick_get_ms() - g_tx_start_ms) >= g_tx_timeout_ms) {
        g_uart2_stats.tx_timeouts++;
        g_uart2_stats.tx_errors++;
        g_tx_result = BSP_UART2_TX_FAILED;
        if (PLATFORM_ERR_OK == plat_uart_abort_tx(BOARD_UART_DEBUG)) {
            g_tx_busy     = 0U;
            g_tx_start_ms = plat_tick_get_ms();
        } else {
            /* Preserve BUSY/storage ownership if cancellation is unconfirmed.
             */
            g_ready = 0U;
        }
    }
    plat_irq_restore(mask);
}

/* end of file -----------------------------------------------------------------------------------*/
