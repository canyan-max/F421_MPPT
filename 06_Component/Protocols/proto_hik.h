/**
 ***************************************************************************************************
 *@file               :   proto_hik.h
 *@brief              :   Legacy DD/A5 HIK read framing and payload encoding.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef PROTO_HIK_H
#define PROTO_HIK_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/
#define PROTO_HIK_BATTERY (0x03U)
#define PROTO_HIK_SOLAR (0x0FU)
#define PROTO_HIK_INFO (0x0CU)
#define PROTO_HIK_SN_READ (0x0DU)
#define PROTO_HIK_SN_WRITE (0xA3U)
#define PROTO_HIK_SN_MAX (16U)
#define PROTO_HIK_MAX_REPLY (40U)
/* Battery reply: 30 fixed bytes plus two bytes per temperature. */
#define PROTO_HIK_NTC_MAX ((PROTO_HIK_MAX_REPLY - 30U) / 2U)

/* typedef ---------------------------------------------------------------------------------------*/
/* Protocol values only; the caller supplies unit conversion and data mapping.
 * These structures are encoded field by field, never sent as raw memory. */
typedef struct PROTO_HIK_BATTERY_T {
    uint16_t voltage_10mv;
    int16_t  current_10ma;
    uint16_t remaining_10mah;
    uint16_t nominal_10mah;
    uint16_t cycle_count;
    uint16_t manufacture_date;
    uint16_t balance_low;
    uint16_t balance_high;
    uint16_t protection;
    uint8_t  software_version;
    uint8_t  soc_percent; /* 0..100 */
    uint8_t  fet_state;   /* Two-bit legacy protocol value: 0..3. */
    uint8_t  cell_count;
    uint8_t  ntc_count;                                  /* 0..PROTO_HIK_NTC_MAX */
    uint16_t temperature_deci_kelvin[PROTO_HIK_NTC_MAX]; /* 0.1 K */
} proto_hik_battery_t;

typedef struct PROTO_HIK_SOLAR_T {
    uint8_t  number;
    uint16_t voltage_10mv;
    uint16_t current_10ma;
} proto_hik_solar_t;

/* Synchronous data providers, invoked only for a valid supported query.
 * Return 1 on success, 0 when data is unavailable. The protocol zeroes the
 * local structure before calling; fill agreed fields in protocol units.
 * The pointer is valid only during the call and must not be retained.
 * Providers run in the caller's context and must be bounded/nonblocking. */
typedef struct PROTO_HIK_PROVIDER_T {
    uint8_t (*pf_get_battery)(proto_hik_battery_t *p_data);
    uint8_t (*pf_get_solar)(proto_hik_solar_t *p_data);
} proto_hik_provider_t;

typedef enum PROTO_HIK_RESULT_T {
    PROTO_HIK_NOT_HIK = 0U,
    PROTO_HIK_INVALID,
    PROTO_HIK_UNSUPPORTED,
    PROTO_HIK_READ,
    PROTO_HIK_WRITE_SN,
} proto_hik_result_t;

/* variables -------------------------------------------------------------------------------------*/

/* function  -------------------------------------------------------------------------------------*/
/* Two's-complement byte sum. Request checksum covers command/length/payload;
 * response checksum covers status/length/payload, excluding response command.
 */
uint16_t           proto_hik_checksum(const uint8_t *p_data, uint16_t length);
proto_hik_result_t proto_hik_parse(const uint8_t *p_data, uint16_t length, uint8_t *p_command);
/* Handles battery/solar reads with caller-supplied data providers.
 * Returns encoded length, or 0 for invalid/unsupported queries, missing or
 * failed providers, invalid data, or insufficient output capacity.
 * No global binding, storage, serial IO, or project dependencies.
 * Provider/output are borrowed only during this call; the caller owns the
 * reply buffer for any subsequent asynchronous send. INFO/SN not handled. */
uint16_t proto_hik_handle(const uint8_t              *p_request,
                          uint16_t                    request_length,
                          const proto_hik_provider_t *p_provider,
                          uint8_t                    *p_output,
                          uint16_t                    capacity);
uint16_t
proto_hik_build_battery(const proto_hik_battery_t *p_data, uint8_t *p_output, uint16_t capacity);
uint16_t
proto_hik_build_solar(const proto_hik_solar_t *p_data, uint8_t *p_output, uint16_t capacity);
uint16_t proto_hik_build_info(uint8_t *p_output, uint16_t capacity);
uint16_t
proto_hik_build_sn(const uint8_t *p_sn, uint16_t length, uint8_t *p_output, uint16_t capacity);
/* User-confirmed seven-byte A3 acknowledgment: DD A3 00 00 00 00 77. */
uint16_t proto_hik_build_sn_ack(uint8_t *p_output, uint16_t capacity);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_HIK_H */
/* end of file -----------------------------------------------------------------------------------*/
