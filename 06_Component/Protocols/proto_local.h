/**
 ***************************************************************************************************
 *@file               :   proto_local.h
 *@brief              :   Local Modbus RTU framing and register callbacks.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ***************************************************************************************************
 */

/* define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef PROTO_LOCAL_H
#define PROTO_LOCAL_H

/* includes --------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // end of __cplusplus
/* define ----------------------------------------------------------------------------------------*/

#define PROTO_LOCAL_ADDRESS (0x15U)
#define PROTO_LOCAL_READ (0x03U)
#define PROTO_LOCAL_WRITE (0x06U)
#define PROTO_LOCAL_FIRST_REGISTER (0x0008U)
#define PROTO_LOCAL_LAST_REGISTER (0x0015U)
#define PROTO_LOCAL_REGISTER_COUNT \
    (PROTO_LOCAL_LAST_REGISTER - PROTO_LOCAL_FIRST_REGISTER + 1U)
#define PROTO_LOCAL_MAX_REPLY (5U + 2U * PROTO_LOCAL_REGISTER_COUNT)

/* typedef ---------------------------------------------------------------------------------------*/

/* Wire addresses of contiguous registers, not byte offsets. */
typedef enum PROTO_LOCAL_REGISTER_T {
    PROTO_LOCAL_REG_INPUT_VOLTAGE = PROTO_LOCAL_FIRST_REGISTER,
    PROTO_LOCAL_REG_INPUT_CURRENT,
    PROTO_LOCAL_REG_BATTERY_VOLTAGE,
    PROTO_LOCAL_REG_BATTERY_CURRENT,
    PROTO_LOCAL_REG_LOAD_VOLTAGE,
    PROTO_LOCAL_REG_LOAD_CURRENT,
    PROTO_LOCAL_REG_TEMPERATURE,
    PROTO_LOCAL_REG_SOC,
    PROTO_LOCAL_REG_CHARGE_POWER,
    PROTO_LOCAL_REG_DISCHARGE_POWER,
    PROTO_LOCAL_REG_DAILY_CHARGE,
    PROTO_LOCAL_REG_DAILY_DISCHARGE,
    PROTO_LOCAL_REG_CHARGE_STATE,
    PROTO_LOCAL_REG_PROTECTION_STATE,
} proto_local_register_t;

typedef enum PROTO_LOCAL_STATUS_T {
    PROTO_LOCAL_OK               = 0U,
    PROTO_LOCAL_ILLEGAL_FUNCTION = 1U,
    PROTO_LOCAL_ILLEGAL_ADDRESS  = 2U,
    PROTO_LOCAL_ILLEGAL_VALUE    = 3U,
    PROTO_LOCAL_DEVICE_FAILURE   = 4U,
} proto_local_status_t;

/* Main-context synchronous providers; no pointer may be retained.
 * Read fills exactly count host-order register words from one snapshot.
 * Signed register values use their 16-bit two's-complement encoding.
 * Write must validate permission/value and apply the change before OK.
 * NULL write callback rejects all writes with ILLEGAL_ADDRESS. */
typedef struct {
    proto_local_status_t (*pf_read_registers)(uint16_t address, uint16_t count, uint16_t *p_values);
    proto_local_status_t (*pf_write_register)(uint16_t address, uint16_t value);
} proto_local_provider_t;

/* variables -------------------------------------------------------------------------------------*/

/* function  -------------------------------------------------------------------------------------*/
/* CRC16/MODBUS: initial FFFF, reflected polynomial A001, no final XOR.
 * NULL returns 0; a non-NULL empty buffer returns FFFF. */
uint16_t proto_local_crc16(const uint8_t *p_data, uint16_t length);
/* One complete RTU frame, no serial IO or persistent storage. Output must
 * not overlap input. Return reply length or 0 for bad CRC/length/address,
 * broadcast, response frames, or insufficient output capacity.
 * Read addresses 0008..0015; quantity 1..125, within this device's block.
 * Provider errors generate exception responses; missing read means failure.
 * Only 03/06 supported; unknown request functions generate exception 01.
 * Broadcast is currently ignored: this project has no writable registers. */
uint16_t proto_local_handle(const uint8_t                *p_request,
                            uint16_t                      length,
                            const proto_local_provider_t *p_provider,
                            uint8_t                      *p_output,
                            uint16_t                      capacity);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PROTO_LOCAL_H */
/* end of file -----------------------------------------------------------------------------------*/
