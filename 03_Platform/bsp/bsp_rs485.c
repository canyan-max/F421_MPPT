/**
 ***************************************************************************************************
 *@file               :   bsp_rs485.c
 *@brief              :   Manage RS485 silence boundaries and half duplex IO.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* includes --------------------------------------------------------------------------------------*/
#include "bsp_rs485.h"
#include "plat_uart.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
#include "kfifo.h"
/* define   --------------------------------------------------------------------------------------*/
#define RS485_SEGMENT_SLOTS (4U)
#define RS485_COPY_CHUNK (16U)
/* 9600/8N1 needs <2 ms per byte; include generous bounded trial margin. */
#define RS485_TX_MARGIN_MS (20U)

/* typedef ---------------------------------------------------------------------------------------*/
typedef struct {
    uint32_t end;
    uint8_t  damaged;
} rs485_segment_t;

typedef enum RS485_TX_STATE_T {
    RS485_TX_IDLE = 0U,
    RS485_TX_SETUP,
    RS485_TX_ACTIVE,
} rs485_tx_state_t;

/* variables -------------------------------------------------------------------------------------*/
volatile bsp_rs485_stats_t       g_rs485_stats;
static kfifo_t                   g_rx_fifo;
static uint8_t                   g_rx_buffer[BSP_RS485_BUFFER_SIZE];
static rs485_segment_t           g_segments[RS485_SEGMENT_SLOTS];
static volatile uint32_t         g_segment_in;
static volatile uint32_t         g_segment_out;
static uint32_t                  g_segment_start;
static uint32_t                  g_rx_last_ms;
static uint8_t                   g_rx_active;
static uint8_t                   g_rx_damaged;
static volatile uint8_t          g_ready;
static bsp_rs485_tx_result_t     g_tx_result;
static volatile rs485_tx_state_t g_tx_state;
static const uint8_t            *p_tx_buffer;
static uint16_t                  g_tx_count;
static uint32_t                  g_tx_start_ms;
static uint32_t                  g_tx_timeout_ms;

/* private  functions  ---------------------------------------------------------------------------*/
/* Called with the original interrupt mask saved; no hardware waits. */
static void fail_tx(void) {
    g_tx_result = BSP_RS485_TX_FAILED;
    g_rs485_stats.tx_errors++;
    if (PLATFORM_ERR_OK != plat_gpio_write(BOARD_GPIO_RS485_TXEN, PLAT_GPIO_RESET)) {
        g_ready = 0U;
    }
    if (RS485_TX_ACTIVE == g_tx_state && PLATFORM_ERR_OK != plat_uart_abort_tx(BOARD_UART_RS485)) {
        /* Do not release storage if cancellation was not confirmed. */
        g_ready = 0U;
        return;
    }
    g_tx_state  = RS485_TX_IDLE;
    p_tx_buffer = NULL;
    /* Let an already loaded UART byte finish before another send. */
    g_tx_start_ms = plat_tick_get_ms();
}

/* Caller serializes UART/SysTick/main with a short saved-mask section. */
static void close_segment(void) {
    uint32_t slot;

    if (0U == g_rx_active) {
        return;
    }
    if ((uint32_t)(g_segment_in - g_segment_out) >= RS485_SEGMENT_SLOTS) {
        /* No consumer can read this unpublished tail. Roll it back rather
         * than mixing it with the next segment or overwriting older data. */
        g_rx_fifo.in = g_segment_start;
        g_rs485_stats.rx_overflows++;
        g_rs485_stats.discarded_segments++;
    } else {
        slot                     = g_segment_in % RS485_SEGMENT_SLOTS;
        g_segments[slot].end     = g_rx_fifo.in;
        g_segments[slot].damaged = g_rx_damaged;
        g_segment_in++;
        g_rs485_stats.rx_segments++;
    }
    g_segment_start = g_rx_fifo.in;
    g_rx_active     = 0U;
    g_rx_damaged    = 0U;
}

static void uart_event(plat_uart_id_t id, plat_uart_event_t event, uint32_t value) {
    uint32_t now_ms;
    uint8_t  byte;

    if (BOARD_UART_RS485 != id || 0U == g_ready) {
        return;
    }
    if (PLAT_UART_EVENT_TX_DONE == event) {
        if (RS485_TX_ACTIVE != g_tx_state) {
            return;
        }
        if (PLATFORM_ERR_OK != plat_gpio_write(BOARD_GPIO_RS485_TXEN, PLAT_GPIO_RESET)) {
            g_rs485_stats.tx_errors++;
            g_tx_result = BSP_RS485_TX_FAILED;
            g_ready     = 0U;
        } else {
            g_tx_result = BSP_RS485_TX_DONE;
            g_rs485_stats.tx_segments++;
        }
        g_tx_state  = RS485_TX_IDLE;
        p_tx_buffer = NULL;
        return;
    }
    if (RS485_TX_IDLE != g_tx_state) {
        return;
    }
    now_ms = plat_tick_get_ms();
    /* Also split here if SysTick's lower priority delayed its publication. */
    if (0U != g_rx_active && (uint32_t)(now_ms - g_rx_last_ms) >= BSP_RS485_RX_GAP_MS) {
        close_segment();
    }
    g_rx_active  = 1U;
    g_rx_last_ms = now_ms;
    if (PLAT_UART_EVENT_RX_ERROR == event) {
        g_rx_damaged = 1U;
        g_rs485_stats.rx_errors++;
        return;
    }
    g_rs485_stats.rx_bytes++;
    if (0U != g_rx_damaged) {
        return;
    }
    byte = (uint8_t)value;
    if (1U != kfifo_put(&g_rx_fifo, &byte, 1U)) {
        g_rx_damaged = 1U;
        g_rs485_stats.rx_overflows++;
    }
}

/* exported functions ----------------------------------------------------------------------------*/
platform_err_t bsp_rs485_init(void) {
    platform_err_t status;
    uint32_t       mask;

    if (0U != g_ready) {
        return PLATFORM_ERR_BUSY;
    }
    status = plat_gpio_write(BOARD_GPIO_RS485_TXEN, PLAT_GPIO_RESET);
    if (PLATFORM_ERR_OK != status) {
        return status;
    }
    if (0U != kfifo_init(&g_rx_fifo, g_rx_buffer, sizeof(g_rx_buffer))) {
        return PLATFORM_ERR_PARAM;
    }
    mask    = plat_irq_save();
    g_ready = 1U;
    status  = plat_uart_start(BOARD_UART_RS485, uart_event);
    if (PLATFORM_ERR_OK != status) {
        g_ready = 0U;
    }
    plat_irq_restore(mask);
    return status;
}

void bsp_rs485_tick_1ms(void) {
    uint32_t mask;
    uint32_t now_ms;

    if (0U == g_ready) {
        return;
    }
    /* UART priority 2 can preempt SysTick priority 15. Serialize the time
     * check and endpoint publication; never copy data in this section. */
    mask   = plat_irq_save();
    now_ms = plat_tick_get_ms();
    if (0U != g_rx_active && (uint32_t)(now_ms - g_rx_last_ms) >= BSP_RS485_RX_GAP_MS) {
        close_segment();
    }
    plat_irq_restore(mask);
}

bsp_rs485_rx_result_t
bsp_rs485_read_segment(uint8_t *p_data, uint16_t capacity, uint16_t *p_count) {
    rs485_segment_t segment;
    uint32_t        count;
    uint32_t        copied = 0U;
    uint32_t        chunk;
    uint32_t        actual;
    uint32_t        mask;
    uint8_t         discard[RS485_COPY_CHUNK];

    if (NULL == p_data || NULL == p_count || 0U == capacity) {
        return BSP_RS485_RX_PARAM;
    }
    *p_count = 0U;
    mask     = plat_irq_save();
    if (0U == g_ready) {
        plat_irq_restore(mask);
        return BSP_RS485_RX_ERROR;
    }
    if (g_segment_out == g_segment_in) {
        plat_irq_restore(mask);
        return BSP_RS485_RX_EMPTY;
    }
    segment = g_segments[g_segment_out % RS485_SEGMENT_SLOTS];
    count   = segment.end - g_rx_fifo.out;
    if (count > BSP_RS485_BUFFER_SIZE || count > kfifo_len(&g_rx_fifo)) {
        g_ready = 0U;
        plat_irq_restore(mask);
        return BSP_RS485_RX_ERROR;
    }
    plat_irq_restore(mask);
    if (0U == segment.damaged && capacity < count) {
        *p_count = (uint16_t)count;
        return BSP_RS485_RX_BUFFER_SMALL;
    }
    while (copied < count) {
        chunk = count - copied;
        if (chunk > RS485_COPY_CHUNK) {
            chunk = RS485_COPY_CHUNK;
        }
        /* Existing kfifo has plain indexes. Serialize only tiny chunks,
         * without changing the user's component or masking a full frame. */
        mask   = plat_irq_save();
        actual = kfifo_get(&g_rx_fifo, 0U != segment.damaged ? discard : p_data + copied, chunk);
        if (actual != chunk) {
            g_ready = 0U;
            plat_irq_restore(mask);
            return BSP_RS485_RX_ERROR;
        }
        plat_irq_restore(mask);
        copied += actual;
    }
    mask = plat_irq_save();
    g_segment_out++;
    if (0U != segment.damaged) {
        g_rs485_stats.discarded_segments++;
    }
    plat_irq_restore(mask);
    if (0U != segment.damaged) {
        return BSP_RS485_RX_DROPPED;
    }
    *p_count = (uint16_t)count;
    return BSP_RS485_RX_OK;
}

platform_err_t bsp_rs485_send_async(const uint8_t *p_data, uint16_t count) {
    uint32_t       mask;
    platform_err_t status;

    if (NULL == p_data || 0U == count || count > BSP_RS485_BUFFER_SIZE) {
        return PLATFORM_ERR_PARAM;
    }
    mask = plat_irq_save();
    if (0U == g_ready) {
        plat_irq_restore(mask);
        return PLATFORM_ERR_HW;
    }
    if (RS485_TX_IDLE != g_tx_state || 0U != g_rx_active ||
        (BSP_RS485_TX_FAILED == g_tx_result &&
         (uint32_t)(plat_tick_get_ms() - g_tx_start_ms) < BSP_RS485_RX_GAP_MS)) {
        plat_irq_restore(mask);
        return PLATFORM_ERR_BUSY;
    }
    status = plat_gpio_write(BOARD_GPIO_RS485_TXEN, PLAT_GPIO_SET);
    if (PLATFORM_ERR_OK != status) {
        g_rs485_stats.tx_errors++;
        g_ready = 0U;
        plat_irq_restore(mask);
        return PLATFORM_ERR_HW;
    }
    p_tx_buffer     = p_data;
    g_tx_count      = count;
    g_tx_start_ms   = plat_tick_get_ms();
    g_tx_timeout_ms = (uint32_t)count * 2U + RS485_TX_MARGIN_MS;
    g_tx_state      = RS485_TX_SETUP;
    g_tx_result     = BSP_RS485_TX_BUSY;
    plat_irq_restore(mask);
    return PLATFORM_ERR_OK;
}

bsp_rs485_tx_result_t bsp_rs485_tx_get_result(void) {
    uint32_t              mask   = plat_irq_save();
    bsp_rs485_tx_result_t result = RS485_TX_IDLE != g_tx_state ? BSP_RS485_TX_BUSY : g_tx_result;

    plat_irq_restore(mask);
    return result;
}

void bsp_rs485_process(void) {
    uint32_t       mask;
    uint32_t       elapsed_ms;
    platform_err_t status;

    if (0U == g_ready) {
        return;
    }
    mask = plat_irq_save();
    if (RS485_TX_IDLE == g_tx_state) {
        plat_irq_restore(mask);
        return;
    }
    elapsed_ms = (uint32_t)(plat_tick_get_ms() - g_tx_start_ms);
    if (elapsed_ms >= g_tx_timeout_ms) {
        g_rs485_stats.tx_timeouts++;
        fail_tx();
    } else if (RS485_TX_SETUP == g_tx_state && elapsed_ms >= BSP_RS485_TX_SETUP_MS) {
        status = plat_uart_send_async(BOARD_UART_RS485, p_tx_buffer, g_tx_count);
        if (PLATFORM_ERR_OK != status) {
            g_ready = 0U;
            fail_tx();
        } else {
            g_tx_state = RS485_TX_ACTIVE;
        }
    }
    plat_irq_restore(mask);
}

/* end of file -----------------------------------------------------------------------------------*/
