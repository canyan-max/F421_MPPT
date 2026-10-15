/**
 ******************************************************************************
 *@file               :   board_measure_config.h
 *@brief              :   Define nominal board measurement conversion parameters.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef BOARD_MEASURE_CONFIG_H
#define BOARD_MEASURE_CONFIG_H
/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
/* define -------------------------------------------------------------------*/
/* ADC reference is calculated from each frame VREFINT sample in BSP. */
/* 12-bit ADC: 4096 quantization levels, codes 0..4095. */
#define BOARD_MEASURE_ADC_LEVELS (4096U)
#define BOARD_MEASURE_ADC_MAX (4095U)
/* R16 = 100 kohm, R15 = 10 kohm. Nominal, not calibrated. */
#define BOARD_MEASURE_BAT_GAIN         (11U)
/* Q3 PNP model: Vin = 20 * Vadc + 2 * VEB. VEB assumed 650 mV.
 * Approximation in the conducting region, not valid near zero/cutoff. */
#define BOARD_MEASURE_VIN_GAIN         (20U)
#define BOARD_MEASURE_VIN_OFFSET_MV    (1300U)
/* U3/U4 INA180A2 gain 50, R6/R8 = 5 mohm: I[mA] = Vadc[mV] * 4.
 * R6/IN_CUR measures Buck output into the battery/load bus, not PV input.
 * R8/OUT_CUR measures the external load branch.
 * Positive sense direction only; output saturation is not reverse current. */
#define BOARD_MEASURE_IN_CURRENT_MA_PER_MV (4U)
#define BOARD_MEASURE_OUT_CURRENT_MA_PER_MV (4U)
/* Datasheet typical 1.20 V (1.16..1.24 V), not per-device calibrated. */
#define BOARD_MEASURE_VREFINT_NOMINAL_MV (1200U)
/* NTC to GND, 100 kohm pull-up to ADC VDDA; nominal B model. */
#define BOARD_MEASURE_NTC_PULLUP_OHM (100000.0f)
#define BOARD_MEASURE_NTC_R25_OHM (10000.0f)
#define BOARD_MEASURE_NTC_BETA_K (3950.0f)
#define BOARD_MEASURE_NTC_T25_K (298.15f)
/* typedef ------------------------------------------------------------------*/
/* variables ----------------------------------------------------------------*/
/* function  ----------------------------------------------------------------*/
/* end of file --------------------------------------------------------------*/
#endif /* BOARD_MEASURE_CONFIG_H */



