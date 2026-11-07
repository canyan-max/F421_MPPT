/**
 ***************************************************************************************************
 *@file               :   proto_hik.c
 *@brief              :   Bounded HIK codecs, no serial or business side
 * effects.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* includes --------------------------------------------------------------------------------------*/
#include "proto_hik.h"
/* define   --------------------------------------------------------------------------------------*/

/* typedef ---------------------------------------------------------------------------------------*/

/* variables -------------------------------------------------------------------------------------*/

/* private  functions  ---------------------------------------------------------------------------*/
static void put_u16(uint8_t *p_output, uint16_t value) {
    p_output[0U] = (uint8_t)(value >> 8U);
    p_output[1U] = (uint8_t)value;
}
static uint16_t finish_reply(uint8_t command, uint8_t payload_length, uint8_t *p_output) {
    uint16_t checksum;
    p_output[0U] = 0xDDU;
    p_output[1U] = command;
    p_output[2U] = 0U;
    p_output[3U] = payload_length;
    checksum     = proto_hik_checksum(p_output + 2U, payload_length + 2U);
    put_u16(p_output + payload_length + 4U, checksum);
    p_output[payload_length + 6U] = 0x77U;
    return payload_length + 7U;
}

/* exported functions ----------------------------------------------------------------------------*/
uint16_t proto_hik_checksum(const uint8_t *p_data, uint16_t length) {
    uint16_t index;
    uint16_t sum = 0U;
    if (NULL == p_data) {
        return 0U;
    }
    for (index = 0U; index < length; index++) {
        sum = (uint16_t)(sum + p_data[index]);
    }
    return (uint16_t)(0U - sum);
}
proto_hik_result_t proto_hik_parse(const uint8_t *p_data, uint16_t length, uint8_t *p_command) {
    uint16_t checksum;
    uint16_t expected;
    if (NULL == p_data || 0U == length || 0xDDU != p_data[0U]) {
        return PROTO_HIK_NOT_HIK;
    }
    if (NULL == p_command || length < 7U || length > 256U || (uint16_t)p_data[3U] + 7U != length ||
        0x77U != p_data[length - 1U]) {
        return PROTO_HIK_INVALID;
    }
    checksum = proto_hik_checksum(p_data + 2U, length - 5U);
    expected = ((uint16_t)p_data[length - 3U] << 8U) | p_data[length - 2U];
    if (checksum != expected) {
        return PROTO_HIK_INVALID;
    }
    if (0x5AU == p_data[1U] && PROTO_HIK_SN_WRITE == p_data[2U]) {
        if (PROTO_HIK_SN_MAX != p_data[3U]) {
            return PROTO_HIK_INVALID;
        }
        *p_command = PROTO_HIK_SN_WRITE;
        return PROTO_HIK_WRITE_SN;
    }
    if (0xA5U != p_data[1U] || (PROTO_HIK_BATTERY != p_data[2U] && PROTO_HIK_SOLAR != p_data[2U] &&
                                PROTO_HIK_INFO != p_data[2U] && PROTO_HIK_SN_READ != p_data[2U])) {
        return PROTO_HIK_UNSUPPORTED;
    }
    /* The reference read handler accepts checksum-valid optional payloads
     * (for example 0F 01 01) without using them to change the reply layout. */
    *p_command = p_data[2U];
    return PROTO_HIK_READ;
}
uint16_t
proto_hik_build_battery(const proto_hik_battery_t *p_data, uint8_t *p_output, uint16_t capacity) {
    uint16_t index;
    uint8_t  length;
    if (NULL == p_data || NULL == p_output || p_data->ntc_count > PROTO_HIK_NTC_MAX ||
        p_data->soc_percent > 100U || p_data->fet_state > 3U) {
        return 0U;
    }
    length = (uint8_t)(23U + 2U * p_data->ntc_count);
    if (capacity < length + 7U) {
        return 0U;
    }
    put_u16(p_output + 4U, p_data->voltage_10mv);
    put_u16(p_output + 6U, (uint16_t)p_data->current_10ma);
    put_u16(p_output + 8U, p_data->remaining_10mah);
    put_u16(p_output + 10U, p_data->nominal_10mah);
    put_u16(p_output + 12U, p_data->cycle_count);
    put_u16(p_output + 14U, p_data->manufacture_date);
    put_u16(p_output + 16U, p_data->balance_low);
    put_u16(p_output + 18U, p_data->balance_high);
    put_u16(p_output + 20U, p_data->protection);
    p_output[22U] = p_data->software_version;
    p_output[23U] = p_data->soc_percent;
    p_output[24U] = p_data->fet_state;
    p_output[25U] = p_data->cell_count;
    p_output[26U] = p_data->ntc_count;
    for (index = 0U; index < p_data->ntc_count; index++) {
        put_u16(p_output + 27U + index * 2U, p_data->temperature_deci_kelvin[index]);
    }
    return finish_reply(PROTO_HIK_BATTERY, length, p_output);
}
uint16_t
proto_hik_build_solar(const proto_hik_solar_t *p_data, uint8_t *p_output, uint16_t capacity) {
    if (NULL == p_data || NULL == p_output || capacity < 12U) {
        return 0U;
    }
    p_output[4U] = p_data->number;
    put_u16(p_output + 5U, p_data->voltage_10mv);
    put_u16(p_output + 7U, p_data->current_10ma);
    return finish_reply(PROTO_HIK_SOLAR, 5U, p_output);
}
uint16_t proto_hik_build_info(uint8_t *p_output, uint16_t capacity) {
    if (NULL == p_output || capacity < 12U) {
        return 0U;
    }
    /* Exact existing 0C capability payload, not the new MCU firmware version.
     */
    p_output[4U] = 4U;
    p_output[5U] = 1U;
    p_output[6U] = 1U;
    p_output[7U] = 0U;
    p_output[8U] = 0U;
    return finish_reply(PROTO_HIK_INFO, 5U, p_output);
}
uint16_t
proto_hik_build_sn(const uint8_t *p_sn, uint16_t length, uint8_t *p_output, uint16_t capacity) {
    uint16_t index;
    if (NULL == p_output || length > PROTO_HIK_SN_MAX || (0U != length && NULL == p_sn) ||
        capacity < length + 7U) {
        return 0U;
    }
    for (index = 0U; index < length; index++) {
        p_output[4U + index] = p_sn[index];
    }
    return finish_reply(PROTO_HIK_SN_READ, (uint8_t)length, p_output);
}
uint16_t proto_hik_build_sn_ack(uint8_t *p_output, uint16_t capacity) {
    if (NULL == p_output || capacity < 7U) {
        return 0U;
    }
    return finish_reply(PROTO_HIK_SN_WRITE, 0U, p_output);
}

uint16_t proto_hik_handle(const uint8_t              *p_request,
                          uint16_t                    request_length,
                          const proto_hik_provider_t *p_provider,
                          uint8_t                    *p_output,
                          uint16_t                    capacity) {
    uint8_t             command;
    proto_hik_battery_t battery = {0};
    proto_hik_solar_t   solar   = {0};

    if (NULL == p_provider || NULL == p_output || 0U == capacity ||
        PROTO_HIK_READ != proto_hik_parse(p_request, request_length, &command)) {
        return 0U;
    }
    switch (command) {
        case PROTO_HIK_BATTERY:
            if (capacity < 30U || NULL == p_provider->pf_get_battery ||
                0U == p_provider->pf_get_battery(&battery)) {
                return 0U;
            }
            return proto_hik_build_battery(&battery, p_output, capacity);

        case PROTO_HIK_SOLAR:
            if (capacity < 12U || NULL == p_provider->pf_get_solar ||
                0U == p_provider->pf_get_solar(&solar)) {
                return 0U;
            }
            return proto_hik_build_solar(&solar, p_output, capacity);

        default:
            return 0U;
    }
}

/* end of file -----------------------------------------------------------------------------------*/
