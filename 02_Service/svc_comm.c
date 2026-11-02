/**
 ******************************************************************************
 *@file               :   svc_comm.c
 *@brief              :   Map live measurements to bounded HIK query replies.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_comm.h"
#include "svc_measure.h"
#include "bsp_rs485.h"
#include "bsp_uart2.h"
#include "proto_hik.h"
/* define   -----------------------------------------------------------------*/

#define COMM_CHANNEL_COUNT (2U)
#define COMM_CHANNEL_RS485 (0U)
#define COMM_CHANNEL_UART2 (1U)

/* typedef ------------------------------------------------------------------*/
typedef struct
{
    uint8_t        rx_buffer[BSP_RS485_BUFFER_SIZE];
    uint8_t        tx_buffer[PROTO_HIK_MAX_REPLY];
    uint16_t       tx_length;
    platform_err_t init_status;
} comm_channel_t;

/* variables ----------------------------------------------------------------*/

/* Each UART owns its encoded reply until TDC or confirmed abort. */
static comm_channel_t g_comm_channels[COMM_CHANNEL_COUNT];
static uint8_t        g_initialized;

/* private  functions  ------------------------------------------------------*/

/* Saturate before narrowing; all protocol currents/voltages use 10-unit steps.
 */
static uint16_t to_u16_units(uint32_t value)
{
    value /= 10U;
    return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static uint16_t
build_reply(uint8_t command, uint8_t *p_output, uint16_t capacity)
{
    svc_measure_value_t value;
    int32_t             converted;
    proto_hik_battery_t battery = {0};
    proto_hik_solar_t   solar   = {0};

    if(PROTO_HIK_BATTERY != command && PROTO_HIK_SOLAR != command)
    {
        return 0U;
    }
    if(PLATFORM_ERR_OK != svc_measure_get(&value))
    {
        /* Do not report zero-filled measurements as a successful sample. */
        return 0U;
    }
    if(PROTO_HIK_SOLAR == command)
    {
        solar.voltage_10mv = to_u16_units(value.input_voltage_mv);
        /* User-selected compatibility mapping: Buck output, not PV current. */
        solar.current_10ma = to_u16_units(value.input_current_ma);
        return proto_hik_build_solar(&solar, p_output, capacity);
    }
    battery.voltage_10mv = to_u16_units(value.battery_voltage_mv);
    converted            = value.battery_current_ma / 10;
    if(converted > INT16_MAX)
    {
        converted = INT16_MAX;
    }
    else if(converted < INT16_MIN)
    {
        converted = INT16_MIN;
    }
    battery.current_10ma = (int16_t)converted;
    if(SVC_MEASURE_TEMPERATURE_INVALID != value.temperature_deci_c)
    {
        converted = (int32_t)value.temperature_deci_c + 2731;
        if(converted >= 0)
        {
            /* Legacy temperature unit is 0.1 K; truncate 273.15 K offset. */
            battery.ntc_count                   = 1U;
            battery.temperature_deci_kelvin[0U] = (uint16_t)converted;
        }
    }
    /* Fields without an agreed source/mapping remain zero. */
    return proto_hik_build_battery(&battery, p_output, capacity);
}

static void process_channel(uint8_t channel)
{
    comm_channel_t *p_channel = &g_comm_channels[channel];
    uint16_t        count;
    uint8_t         command;
    uint8_t         received;
    platform_err_t  status;

    if(PLATFORM_ERR_OK != p_channel->init_status)
    {
        return;
    }
    if(COMM_CHANNEL_RS485 == channel)
    {
        bsp_rs485_process();
        if(BSP_RS485_TX_BUSY == bsp_rs485_tx_get_result())
        {
            return;
        }
    }
    else
    {
        bsp_uart2_process();
        if(BSP_UART2_TX_BUSY == bsp_uart2_tx_get_result())
        {
            return;
        }
    }
    if(0U == p_channel->tx_length)
    {
        if(COMM_CHANNEL_RS485 == channel)
        {
            received = BSP_RS485_RX_OK ==
                       bsp_rs485_read_segment(p_channel->rx_buffer,
                                              sizeof(p_channel->rx_buffer),
                                              &count);
        }
        else
        {
            received = BSP_UART2_RX_OK ==
                       bsp_uart2_read_segment(p_channel->rx_buffer,
                                              sizeof(p_channel->rx_buffer),
                                              &count);
        }
        if(0U == received ||
           PROTO_HIK_READ !=
               proto_hik_parse(p_channel->rx_buffer, count, &command))
        {
            return;
        }
        p_channel->tx_length = build_reply(command, p_channel->tx_buffer,
                                           sizeof(p_channel->tx_buffer));
        if(0U == p_channel->tx_length)
        {
            return;
        }
    }
    if(COMM_CHANNEL_RS485 == channel)
    {
        status = bsp_rs485_send_async(p_channel->tx_buffer,
                                      p_channel->tx_length);
    }
    else
    {
        status = bsp_uart2_send_async(p_channel->tx_buffer,
                                      p_channel->tx_length);
    }
    if(PLATFORM_ERR_BUSY != status)
    {
        /* Accepted sends are never replayed automatically after a failure. */
        p_channel->tx_length = 0U;
    }
}

/* exported functions -------------------------------------------------------*/
platform_err_t svc_comm_init(void)
{
    if(0U != g_initialized)
    {
        return PLATFORM_ERR_BUSY;
    }
    g_initialized                                   = 1U;
    g_comm_channels[COMM_CHANNEL_RS485].init_status = bsp_rs485_init();
    g_comm_channels[COMM_CHANNEL_UART2].init_status = bsp_uart2_init();
    if(PLATFORM_ERR_OK != g_comm_channels[COMM_CHANNEL_RS485].init_status)
    {
        return g_comm_channels[COMM_CHANNEL_RS485].init_status;
    }
    return g_comm_channels[COMM_CHANNEL_UART2].init_status;
}

void svc_comm_process(void)
{
    uint8_t channel;

    if(0U == g_initialized)
    {
        return;
    }
    for(channel = 0U; channel < COMM_CHANNEL_COUNT; channel++)
    {
        process_channel(channel);
    }
}

/* end of file --------------------------------------------------------------*/
