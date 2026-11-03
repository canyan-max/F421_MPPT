/**
 ******************************************************************************
 *@file               :   svc_comm.c
 *@brief              :   Run table-driven communication with per-port storage.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_comm.h"
#include "svc_packdata.h"
#include "bsp_rs485.h"
#include "bsp_uart2.h"
/* define   -----------------------------------------------------------------*/
/* Increase these bounds when adding protocols with larger segments/replies. */
#define COMM_RX_CAPACITY (256U)
#define COMM_TX_CAPACITY (40U)

/* typedef ------------------------------------------------------------------*/
typedef struct
{
    platform_err_t (*pf_init)(void);
    void (*pf_process)(void);
    uint8_t (*pf_tx_busy)(void);
    uint8_t (*pf_read)(uint8_t *p_data, uint16_t capacity, uint16_t *p_count);
    platform_err_t (*pf_send)(const uint8_t *p_data, uint16_t count);
} comm_port_t;

typedef struct
{
    uint8_t        rx_buffer[COMM_RX_CAPACITY];
    uint8_t        tx_buffer[COMM_TX_CAPACITY];
    uint16_t       tx_length;
    platform_err_t init_status;
} comm_channel_t;

static uint8_t rs485_tx_busy(void);
static uint8_t uart2_tx_busy(void);
static uint8_t
rs485_read(uint8_t *p_data, uint16_t capacity, uint16_t *p_count);
static uint8_t
uart2_read(uint8_t *p_data, uint16_t capacity, uint16_t *p_count);

/* variables ----------------------------------------------------------------*/
/* Table index is the service channel ID: 0=UART1/RS485, 1=UART2. */
static const comm_port_t g_comm_ports[] = {
    {bsp_rs485_init, bsp_rs485_process, rs485_tx_busy, rs485_read,
     bsp_rs485_send_async},
    {bsp_uart2_init, bsp_uart2_process, uart2_tx_busy, uart2_read,
     bsp_uart2_send_async},
};

/* Each UART owns its encoded reply until TDC or confirmed abort. */
static comm_channel_t
               g_comm_channels[sizeof(g_comm_ports) / sizeof(g_comm_ports[0U])];
static uint8_t g_initialized;

/* private  functions  ------------------------------------------------------*/
/* Normalize distinct BSP enums without incompatible function-pointer casts. */
static uint8_t rs485_tx_busy(void)
{
    return BSP_RS485_TX_BUSY == bsp_rs485_tx_get_result();
}

static uint8_t uart2_tx_busy(void)
{
    return BSP_UART2_TX_BUSY == bsp_uart2_tx_get_result();
}

static uint8_t rs485_read(uint8_t *p_data, uint16_t capacity, uint16_t *p_count)
{
    return BSP_RS485_RX_OK == bsp_rs485_read_segment(p_data, capacity, p_count);
}

static uint8_t uart2_read(uint8_t *p_data, uint16_t capacity, uint16_t *p_count)
{
    return BSP_UART2_RX_OK == bsp_uart2_read_segment(p_data, capacity, p_count);
}

static void process_channel(size_t channel)
{
    comm_channel_t    *p_channel = &g_comm_channels[channel];
    const comm_port_t *p_port    = &g_comm_ports[channel];
    uint16_t           count     = 0U;
    platform_err_t     status;

    if(PLATFORM_ERR_OK != p_channel->init_status)
    {
        return;
    }
    p_port->pf_process();
    if(0U != p_port->pf_tx_busy())
    {
        return;
    }
    if(0U == p_channel->tx_length)
    {
        if(0U == p_port->pf_read(p_channel->rx_buffer,
                                 sizeof(p_channel->rx_buffer), &count))
        {
            return;
        }
        p_channel->tx_length = svc_packdata_handle(p_channel->rx_buffer, count,
                                                   p_channel->tx_buffer,
                                                   sizeof(
                                                       p_channel->tx_buffer));
        if(0U == p_channel->tx_length)
        {
            return;
        }
    }
    status = p_port->pf_send(p_channel->tx_buffer, p_channel->tx_length);
    if(PLATFORM_ERR_BUSY != status)
    {
        /* Accepted sends are never replayed automatically after a failure. */
        p_channel->tx_length = 0U;
    }
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_comm_init(void)
{
    size_t         channel;
    platform_err_t first_error = PLATFORM_ERR_OK;

    if(0U != g_initialized)
    {
        return PLATFORM_ERR_BUSY;
    }
    g_initialized = 1U;
    for(channel = 0U; channel < sizeof(g_comm_ports) / sizeof(g_comm_ports[0U]);
        channel++)
    {
        g_comm_channels[channel].init_status = g_comm_ports[channel].pf_init();
        if(PLATFORM_ERR_OK == first_error &&
           PLATFORM_ERR_OK != g_comm_channels[channel].init_status)
        {
            first_error = g_comm_channels[channel].init_status;
        }
    }
    return first_error;
}

void svc_comm_process(void)
{
    size_t channel;

    if(0U == g_initialized)
    {
        return;
    }
    for(channel = 0U; channel < sizeof(g_comm_ports) / sizeof(g_comm_ports[0U]);
        channel++)
    {
        process_channel(channel);
    }
}

/* end of file --------------------------------------------------------------*/
