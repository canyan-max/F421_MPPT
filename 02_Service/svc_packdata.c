/**
 ******************************************************************************
 *@file               :   svc_packdata.c
 *@brief              :   Bind project data and dispatch queries by protocol ID.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_packdata.h"
#include "svc_measure.h"
#include "proto_hik.h"
#include "proto_local.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* IDs identify the first request byte, not the command byte. */
typedef struct
{
    uint8_t id;
    uint16_t (*pf_handle)(const uint8_t *p_request,
                          uint16_t       length,
                          uint8_t       *p_output,
                          uint16_t       capacity);
} packdata_protocol_t;

static uint8_t  get_hik_battery(proto_hik_battery_t *p_data);
static uint8_t  get_hik_solar(proto_hik_solar_t *p_data);
static uint16_t handle_hik(const uint8_t *p_request,
                           uint16_t       length,
                           uint8_t       *p_output,
                           uint16_t       capacity);

static proto_local_status_t
read_local_registers(uint16_t address, uint16_t count, uint16_t *p_values);
static uint16_t handle_local(const uint8_t *p_request,
                             uint16_t       length,
                             uint8_t       *p_output,
                             uint16_t       capacity);

/* variables ----------------------------------------------------------------*/

static const proto_hik_provider_t g_hik_provider = {
    get_hik_battery,
    get_hik_solar,
};

/* No writable registers yet; a missing write callback yields exception 02. */
static const proto_local_provider_t g_local_provider = {
    read_local_registers,
    NULL,
};
/* Unique request prefix per row. A claimed frame is never retried as
 * another protocol, including invalid or unsupported queries. */
static const packdata_protocol_t g_protocols[] = {
    {0xDDU, handle_hik},
    {PROTO_LOCAL_ADDRESS, handle_local},
};

/* private  functions  ------------------------------------------------------*/

/* Saturate before narrowing; all protocol currents/voltages use 10-unit steps.
 */
static uint16_t to_u16_units(uint32_t value)
{
    value /= 10U;
    return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static uint8_t get_hik_solar(proto_hik_solar_t *p_data)
{
    svc_measure_value_t value;

    if(NULL == p_data || PLATFORM_ERR_OK != svc_measure_get(&value))
    {
        return 0U;
    }
    p_data->voltage_10mv = to_u16_units(value.input_voltage_mv);
    /* User-selected compatibility mapping: Buck output, not PV current. */
    p_data->current_10ma = to_u16_units(value.input_current_ma);
    return 1U;
}

static uint8_t get_hik_battery(proto_hik_battery_t *p_data)
{
    svc_measure_value_t value;
    int32_t             converted;

    if(NULL == p_data || PLATFORM_ERR_OK != svc_measure_get(&value))
    {
        return 0U;
    }
    p_data->voltage_10mv = to_u16_units(value.battery_voltage_mv);
    converted            = value.battery_current_ma / 10;
    if(converted > INT16_MAX)
    {
        converted = INT16_MAX;
    }
    else if(converted < INT16_MIN)
    {
        converted = INT16_MIN;
    }
    p_data->current_10ma = (int16_t)converted;
    if(SVC_MEASURE_TEMPERATURE_INVALID != value.temperature_deci_c)
    {
        converted = (int32_t)value.temperature_deci_c + 2731;
        if(converted >= 0)
        {
            /* Legacy temperature unit is 0.1 K; truncate 273.15 K offset. */
            p_data->ntc_count                   = 1U;
            p_data->temperature_deci_kelvin[0U] = (uint16_t)converted;
        }
    }
    /* Fields without an agreed source/mapping remain zero. */
    return 1U;
}

static uint16_t handle_hik(const uint8_t *p_request,
                           uint16_t       length,
                           uint8_t       *p_output,
                           uint16_t       capacity)
{
    return proto_hik_handle(p_request, length, &g_hik_provider, p_output,
                            capacity);
}

static uint16_t saturate_u16(uint32_t value)
{
    return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static uint16_t encode_i16(int32_t value)
{
    if(value > INT16_MAX)
    {
        value = INT16_MAX;
    }
    else if(value < INT16_MIN)
    {
        value = INT16_MIN;
    }
    return (uint16_t)value;
}

static proto_local_status_t
read_local_registers(uint16_t address, uint16_t count, uint16_t *p_values)
{
    svc_measure_value_t value;
    uint16_t            index;

    if(NULL == p_values || 0U == count ||
       address < PROTO_LOCAL_FIRST_REGISTER ||
       address > PROTO_LOCAL_LAST_REGISTER ||
       count > PROTO_LOCAL_LAST_REGISTER - address + 1U)
    {
        return PROTO_LOCAL_ILLEGAL_ADDRESS;
    }
    if(PLATFORM_ERR_OK != svc_measure_get(&value))
    {
        return PROTO_LOCAL_DEVICE_FAILURE;
    }
    for(index = 0U; index < count; index++)
    {
        p_values[index] = 0U;
        switch(address + index)
        {
            case PROTO_LOCAL_REG_INPUT_VOLTAGE:
                p_values[index] = saturate_u16(value.input_voltage_mv);
                break;
            case PROTO_LOCAL_REG_INPUT_CURRENT:
                /* User mapping: Buck output current, not PV input current. */
                p_values[index] = saturate_u16(value.input_current_ma);
                break;
            case PROTO_LOCAL_REG_BATTERY_VOLTAGE:
            case PROTO_LOCAL_REG_LOAD_VOLTAGE:
                /* Load voltage is the user-selected battery bus proxy. */
                p_values[index] = saturate_u16(value.battery_voltage_mv);
                break;
            case PROTO_LOCAL_REG_BATTERY_CURRENT:
                p_values[index] = encode_i16(value.battery_current_ma);
                break;
            case PROTO_LOCAL_REG_LOAD_CURRENT:
                p_values[index] = saturate_u16(value.output_current_ma);
                break;
            case PROTO_LOCAL_REG_TEMPERATURE:
                if(SVC_MEASURE_TEMPERATURE_INVALID != value.temperature_deci_c)
                {
                    p_values[index] = encode_i16(value.temperature_deci_c);
                }
                break;
            default:
                /* SOC/power/energy/state definitions await agreement. */
                break;
        }
    }
    return PROTO_LOCAL_OK;
}

static uint16_t handle_local(const uint8_t *p_request,
                             uint16_t       length,
                             uint8_t       *p_output,
                             uint16_t       capacity)
{
    return proto_local_handle(p_request, length, &g_local_provider, p_output,
                              capacity);
}

/* exported functions -------------------------------------------------------*/

uint16_t svc_packdata_handle(const uint8_t *p_request,
                             uint16_t       length,
                             uint8_t       *p_output,
                             uint16_t       capacity)
{
    size_t   index;
    uint16_t reply_length;

    if(NULL == p_request || 0U == length || NULL == p_output || 0U == capacity)
    {
        return 0U;
    }
    for(index = 0U; index < sizeof(g_protocols) / sizeof(g_protocols[0U]);
        index++)
    {
        if(p_request[0U] == g_protocols[index].id)
        {
            reply_length = g_protocols[index].pf_handle(p_request, length,
                                                        p_output, capacity);
            return reply_length <= capacity ? reply_length : 0U;
        }
    }
    return 0U;
}

/* end of file --------------------------------------------------------------*/
