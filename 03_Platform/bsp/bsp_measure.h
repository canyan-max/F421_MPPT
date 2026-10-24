/**
 ******************************************************************************
 *@file               :   bsp_measure.h
 *@brief              :   Convert one coherent raw ADC frame to board units.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BSP_MEASURE_H
#define BSP_MEASURE_H
/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "plat_error.h"
#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/
#define BSP_MEASURE_TEMPERATURE_INVALID (INT16_MIN)
/* typedef ------------------------------------------------------------------*/
/* Board scan indices, not physical ADC channel numbers. */
typedef enum BSP_MEASURE_SAMPLE_INDEX_T
{
    BSP_MEASURE_OUT_CUR = 0U,
    BSP_MEASURE_IN_CUR,
    BSP_MEASURE_BAT_VOL,
    BSP_MEASURE_VIN_VOL,
    BSP_MEASURE_NTC,
    BSP_MEASURE_VREFINT,
    BSP_MEASURE_CHANNEL_COUNT,
} bsp_measure_sample_index_t;

typedef struct BSP_MEASURE_VALUE_T
{
    /* Filtered voltages used by startup, CV control and protection. */
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    /* Filtered currents: IN_CUR is Buck output into the battery/load bus;
     * OUT_CUR is the external load branch, not solar input current. */
    uint32_t input_current_ma;
    uint32_t output_current_ma;
    /* Derived from filtered currents. Positive charges the battery;
     * negative means the battery is supplying the load. */
    int32_t battery_current_ma;
    /* 0.1 degrees Celsius; INVALID means conversion unavailable. */
    int16_t temperature_deci_c;
} bsp_measure_value_t;
typedef enum BSP_MEASURE_ERROR_T
{
    BSP_MEASURE_ERROR_NONE = 0U,
    BSP_MEASURE_ERROR_INIT,
    BSP_MEASURE_ERROR_DMA,
    BSP_MEASURE_ERROR_CONVERT,
    BSP_MEASURE_ERROR_FILTER,
} bsp_measure_error_t;
typedef void (*bsp_measure_data_callback_t)(const bsp_measure_value_t *p_value,
                                            uint32_t sequence);
typedef void (*bsp_measure_error_callback_t)(bsp_measure_error_t error,
                                             platform_err_t      status);
/* variables ----------------------------------------------------------------*/
/* function  ----------------------------------------------------------------*/
/* Main context only. NULL unregisters the corresponding callback.
 * Callbacks execute synchronously inside process(), never inside DMA ISR.
 * p_value is valid only during the callback; copy to retain it.
 * Do not reenter init/process or register callbacks from a callback.
 * Voltages and currents use the available moving-average samples during
 * warm-up.
 * ADC endpoints are converted and published without plausibility checks.
 * Hardware, conversion and filter failures use the error callback.
 * Persistent identical errors notify once until success or re-registration. */
void bsp_measure_set_callbacks(bsp_measure_data_callback_t  pf_data,
                               bsp_measure_error_callback_t pf_error);
/* Main context, 12 V trial only. ISR only turns PWM off; no fault latch or
 * recovery policy. Zero disables this raw-code check for 24 V configuration. */
void bsp_measure_fast_ovp_enable(uint8_t enabled);
/* Startup once after ADC/DMA init and before sampling timer starts. */
platform_err_t bsp_measure_init(void);
/* Nonblocking main-context processing of the latest complete raw frame. */
void bsp_measure_process(void);
#ifdef __cplusplus
}
#endif // end of __cplusplus
#endif /* BSP_MEASURE_H */
/* end of file --------------------------------------------------------------*/
