/**
 ******************************************************************************
 *@file               :   proto_local.c
 *@brief              :   Validate local Modbus RTU frames and encode replies.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "proto_local.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

static uint16_t read_be(const uint8_t *p_data)
{
    return (uint16_t)(((uint16_t)p_data[0U] << 8U) | p_data[1U]);
}

static uint16_t append_crc(uint8_t *p_output, uint16_t length)
{
    uint16_t crc          = proto_local_crc16(p_output, length);
    p_output[length]      = (uint8_t)crc;
    p_output[length + 1U] = (uint8_t)(crc >> 8U);
    return length + 2U;
}

static uint16_t build_exception(uint8_t              function,
                                proto_local_status_t status,
                                uint8_t             *p_output,
                                uint16_t             capacity)
{
    if(capacity < 5U)
    {
        return 0U;
    }
    if(PROTO_LOCAL_ILLEGAL_FUNCTION != status &&
       PROTO_LOCAL_ILLEGAL_ADDRESS != status &&
       PROTO_LOCAL_ILLEGAL_VALUE != status &&
       PROTO_LOCAL_DEVICE_FAILURE != status)
    {
        status = PROTO_LOCAL_DEVICE_FAILURE;
    }
    p_output[0U] = PROTO_LOCAL_ADDRESS;
    p_output[1U] = function | 0x80U;
    p_output[2U] = (uint8_t)status;
    return append_crc(p_output, 3U);
}

static uint16_t handle_read(const uint8_t                *p_request,
                            const proto_local_provider_t *p_provider,
                            uint8_t                      *p_output,
                            uint16_t                      capacity)
{
    uint16_t             address = read_be(&p_request[2U]);
    uint16_t             count   = read_be(&p_request[4U]);
    uint16_t             values[PROTO_LOCAL_REGISTER_COUNT] = {0};
    uint16_t             index;
    proto_local_status_t status;

    if(0U == count || count > 125U)
    {
        return build_exception(PROTO_LOCAL_READ, PROTO_LOCAL_ILLEGAL_VALUE,
                               p_output, capacity);
    }
    /* Subtraction after range checks avoids 16-bit address wraparound. */
    if(address < PROTO_LOCAL_FIRST_REGISTER ||
       address > PROTO_LOCAL_LAST_REGISTER ||
       count > PROTO_LOCAL_LAST_REGISTER - address + 1U)
    {
        return build_exception(PROTO_LOCAL_READ, PROTO_LOCAL_ILLEGAL_ADDRESS,
                               p_output, capacity);
    }
    if(capacity < 5U + 2U * count)
    {
        return 0U;
    }
    if(NULL == p_provider || NULL == p_provider->pf_read_registers)
    {
        status = PROTO_LOCAL_DEVICE_FAILURE;
    }
    else
    {
        status = p_provider->pf_read_registers(address, count, values);
    }
    if(PROTO_LOCAL_OK != status)
    {
        return build_exception(PROTO_LOCAL_READ, status, p_output, capacity);
    }
    p_output[0U] = PROTO_LOCAL_ADDRESS;
    p_output[1U] = PROTO_LOCAL_READ;
    p_output[2U] = (uint8_t)(2U * count);
    for(index = 0U; index < count; index++)
    {
        p_output[3U + 2U * index] = (uint8_t)(values[index] >> 8U);
        p_output[4U + 2U * index] = (uint8_t)values[index];
    }
    return append_crc(p_output, 3U + 2U * count);
}

static uint16_t handle_write(const uint8_t                *p_request,
                             const proto_local_provider_t *p_provider,
                             uint8_t                      *p_output,
                             uint16_t                      capacity)
{
    proto_local_status_t status;
    uint16_t             index;

    if(NULL == p_provider || NULL == p_provider->pf_write_register)
    {
        return build_exception(PROTO_LOCAL_WRITE, PROTO_LOCAL_ILLEGAL_ADDRESS,
                               p_output, capacity);
    }
    /* Never apply a write if its success reply cannot be stored. */
    if(capacity < 8U)
    {
        return 0U;
    }
    status = p_provider->pf_write_register(read_be(&p_request[2U]),
                                           read_be(&p_request[4U]));
    if(PROTO_LOCAL_OK != status)
    {
        return build_exception(PROTO_LOCAL_WRITE, status, p_output, capacity);
    }
    for(index = 0U; index < 6U; index++)
    {
        p_output[index] = p_request[index];
    }
    return append_crc(p_output, 6U);
}

/* exported functions -------------------------------------------------------*/

uint16_t proto_local_crc16(const uint8_t *p_data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;
    uint8_t  bit;
    if(NULL == p_data)
    {
        return 0U;
    }
    for(index = 0U; index < length; index++)
    {
        crc ^= p_data[index];
        for(bit = 0U; bit < 8U; bit++)
        {
            crc = 0U != (crc & 1U) ? (crc >> 1U) ^ 0xA001U : crc >> 1U;
        }
    }
    return crc;
}

uint16_t proto_local_handle(const uint8_t                *p_request,
                            uint16_t                      length,
                            const proto_local_provider_t *p_provider,
                            uint8_t                      *p_output,
                            uint16_t                      capacity)
{
    uint16_t crc;

    if(NULL == p_request || NULL == p_output || length < 4U || length > 256U ||
       capacity < 5U)
    {
        return 0U;
    }
    if(PROTO_LOCAL_ADDRESS != p_request[0U] || 0U != (p_request[1U] & 0x80U))
    {
        return 0U;
    }
    crc = (uint16_t)(p_request[length - 2U] |
                     ((uint16_t)p_request[length - 1U] << 8U));
    if(crc != proto_local_crc16(p_request, length - 2U))
    {
        return 0U;
    }
    switch(p_request[1U])
    {
        case PROTO_LOCAL_READ:
            return 8U == length
                       ? handle_read(p_request, p_provider, p_output, capacity)
                       : 0U;
        case PROTO_LOCAL_WRITE:
            return 8U == length
                       ? handle_write(p_request, p_provider, p_output, capacity)
                       : 0U;
        default:
            return build_exception(p_request[1U], PROTO_LOCAL_ILLEGAL_FUNCTION,
                                   p_output, capacity);
    }
}

/* end of file --------------------------------------------------------------*/
