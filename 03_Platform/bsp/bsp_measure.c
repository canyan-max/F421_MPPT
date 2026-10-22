/**
 ******************************************************************************
 *@file               :   bsp_measure.c
 *@brief              :   Apply nominal board voltage and current conversions.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include "plat_adc.h"
#include "bsp_measure.h"
#include "board_measure_config.h"
#include "board_resources.h"
#include "filter.h"
/* define -------------------------------------------------------------------*/
#define MEASURE_VOLTAGE_FILTER_FRAMES (8U) /* Trial window, not protection. */
#define MEASURE_CURRENT_FILTER_FRAMES (8U) /* Trial window, not protection. */
/* typedef ------------------------------------------------------------------*/
typedef struct BSP_MEASURE_INTERNAL_VALUE_T
{
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    uint32_t input_current_ma;
    uint32_t output_current_ma;
    /* Historical names: IN_CUR = Buck output into battery/load bus;
     * OUT_CUR = external load branch. Neither is solar input current.
     * Steady net battery current approximates IN_CUR minus OUT_CUR. */
    int16_t temperature_deci_c;
} bsp_measure_internal_value_t;
/* variables ----------------------------------------------------------------*/
/* Static lifetime; DMA owns the two alternating six-channel halves. */
volatile uint16_t              g_adc_dma_buffer[2U * BSP_MEASURE_CHANNEL_COUNT];
static volatile uint16_t       g_adc_raw[BSP_MEASURE_CHANNEL_COUNT];
static volatile uint32_t       g_adc_frame_count;
static volatile uint32_t       g_adc_dma_error_count;
static volatile uint32_t       g_adc_overrun_count;
static uint32_t                g_consumed_overrun_count;
static volatile platform_err_t g_measure_status;
static uint32_t                g_consumed_sequence;
static MovingAvg_t             g_input_voltage_filter;
static MovingAvg_t             g_battery_voltage_filter;
static MovingAvg_t             g_input_current_filter;
static MovingAvg_t             g_output_current_filter;
static bsp_measure_data_callback_t  pf_measure_data;
static bsp_measure_error_callback_t pf_measure_error;
static bsp_measure_error_t          g_last_error;
static uint8_t                      g_measure_initialized;
/* private functions --------------------------------------------------------*/
static void report_error(bsp_measure_error_t error, platform_err_t status)
{
    g_measure_status = status;
    if(NULL != pf_measure_error && error != g_last_error)
    {
        g_last_error = error;
        pf_measure_error(error, status);
    }
}

static platform_err_t reset_measure_filter(void)
{
    filter_status_t input_voltage_status = filter_reset(
        &g_input_voltage_filter);
    filter_status_t battery_voltage_status = filter_reset(
        &g_battery_voltage_filter);
    filter_status_t input_current_status = filter_reset(
        &g_input_current_filter);
    filter_status_t output_current_status = filter_reset(
        &g_output_current_filter);
    return (FILTER_OK == input_voltage_status &&
            FILTER_OK == battery_voltage_status &&
            FILTER_OK == input_current_status &&
            FILTER_OK == output_current_status)
               ? PLATFORM_ERR_OK
               : PLATFORM_ERR_HW;
}

static platform_err_t filter_measure(bsp_measure_internal_value_t *p_value)
{
    int32_t input_voltage_average;
    int32_t battery_voltage_average;
    int32_t input_current_average;
    int32_t output_current_average;
    if(NULL == p_value || p_value->input_voltage_mv > INT32_MAX ||
       p_value->battery_voltage_mv > INT32_MAX ||
       p_value->input_current_ma > INT32_MAX ||
       p_value->output_current_ma > INT32_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(FILTER_OK != filter_add(&g_input_voltage_filter,
                               (int32_t)p_value->input_voltage_mv,
                               &input_voltage_average) ||
       FILTER_OK != filter_add(&g_battery_voltage_filter,
                               (int32_t)p_value->battery_voltage_mv,
                               &battery_voltage_average) ||
       FILTER_OK != filter_add(&g_input_current_filter,
                               (int32_t)p_value->input_current_ma,
                               &input_current_average) ||
       FILTER_OK != filter_add(&g_output_current_filter,
                               (int32_t)p_value->output_current_ma,
                               &output_current_average) ||
       input_voltage_average < 0 || battery_voltage_average < 0 ||
       input_current_average < 0 || output_current_average < 0 ||
       g_input_voltage_filter.count != g_battery_voltage_filter.count ||
       g_input_voltage_filter.count != g_input_current_filter.count ||
       g_input_voltage_filter.count != g_output_current_filter.count)
    {
        return PLATFORM_ERR_HW;
    }
    p_value->input_voltage_mv   = (uint32_t)input_voltage_average;
    p_value->battery_voltage_mv = (uint32_t)battery_voltage_average;
    p_value->input_current_ma   = (uint32_t)input_current_average;
    p_value->output_current_ma  = (uint32_t)output_current_average;
    return PLATFORM_ERR_OK;
}
/* ISR only copies raw samples; no conversion or pointer retention. */
static void measure_adc_callback(plat_adc_id_t            id,
                                 plat_adc_event_t         event,
                                 const volatile uint16_t *p_samples,
                                 uint16_t                 count)
{
    uint32_t index = 0U;

    if(BOARD_ADC_MEASURE != id)
    {
        return;
    }
    if(PLAT_ADC_EVENT_ERROR == event)
    {
        g_adc_dma_error_count++;
        return;
    }
    if(PLAT_ADC_EVENT_OVERRUN == event)
    {
        g_adc_overrun_count++;
        return;
    }
    if(NULL == p_samples || BSP_MEASURE_CHANNEL_COUNT != count ||
       (PLAT_ADC_EVENT_HALF != event && PLAT_ADC_EVENT_FULL != event))
    {
        g_adc_dma_error_count++;
        return;
    }
    for(index = 0U; index < BSP_MEASURE_CHANNEL_COUNT; index++)
    {
        g_adc_raw[index] = p_samples[index];
    }
    plat_adc_memory_barrier();
    g_adc_frame_count++;
}

static platform_err_t bsp_measure_adc_voltage_mv(uint16_t  raw,
                                                 uint32_t  reference_mv,
                                                 uint32_t *p_value)
{
    if(NULL == p_value || raw > BOARD_MEASURE_ADC_MAX || 0U == reference_mv)
    {
        return PLATFORM_ERR_PARAM;
    }

    if(0U == raw)
    {
        *p_value = 0U;
        return PLATFORM_ERR_OK;
    }
    /* ADC pin voltage in mV, rounded to nearest integer. */
    *p_value = (uint32_t)(((uint64_t)raw * reference_mv +
                           BOARD_MEASURE_ADC_LEVELS / 2U) /
                          BOARD_MEASURE_ADC_LEVELS);
    return PLATFORM_ERR_OK;
}
static platform_err_t bsp_measure_input_voltage_mv(uint16_t  raw,
                                                   uint32_t  reference_mv,
                                                   uint32_t *p_value)
{
    uint32_t       adc_voltage_mv = 0U;
    uint64_t       converted      = 0U;
    platform_err_t status         = PLATFORM_ERR_OK;

    if(NULL == p_value)
    {
        return PLATFORM_ERR_PARAM;
    }
    status = bsp_measure_adc_voltage_mv(raw, reference_mv, &adc_voltage_mv);
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    if(adc_voltage_mv <= 20U)
    {
        *p_value = 0U;
        return PLATFORM_ERR_OK;
    }
    /* VIN model: input = 20 * ADC pin voltage + trial VEB offset. */
    converted = (uint64_t)adc_voltage_mv * BOARD_MEASURE_VIN_GAIN +
                BOARD_MEASURE_VIN_OFFSET_MV;
    if(converted > UINT32_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_value = (uint32_t)converted;
    return PLATFORM_ERR_OK;
}
static platform_err_t bsp_measure_battery_voltage_mv(uint16_t  raw,
                                                     uint32_t  reference_mv,
                                                     uint32_t *p_value)
{
    uint32_t       adc_voltage_mv = 0U;
    uint64_t       converted      = 0U;
    platform_err_t status         = PLATFORM_ERR_OK;

    if(NULL == p_value)
    {
        return PLATFORM_ERR_PARAM;
    }
    status = bsp_measure_adc_voltage_mv(raw, reference_mv, &adc_voltage_mv);
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    if(0U == adc_voltage_mv)
    {
        *p_value = 0U;
        return PLATFORM_ERR_OK;
    }
    /* Battery divider: (100 kohm + 10 kohm) / 10 kohm = 11. */
    converted = (uint64_t)adc_voltage_mv * BOARD_MEASURE_BAT_GAIN + 0U;
    if(converted > UINT32_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_value = (uint32_t)converted;
    return PLATFORM_ERR_OK;
}
static platform_err_t bsp_measure_input_current_ma(uint16_t  raw,
                                                   uint32_t  reference_mv,
                                                   uint32_t *p_value)
{
    uint32_t       adc_voltage_mv = 0U;
    uint64_t       converted      = 0U;
    platform_err_t status         = PLATFORM_ERR_OK;

    if(NULL == p_value)
    {
        return PLATFORM_ERR_PARAM;
    }
    status = bsp_measure_adc_voltage_mv(raw, reference_mv, &adc_voltage_mv);
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    if(0U == adc_voltage_mv)
    {
        *p_value = 0U;
        return PLATFORM_ERR_OK;
    }
    /* IN_CUR: Buck output into battery/load bus, not PV input current.
     * INA180A2 gain 50, shunt 5 mohm: current mA = pin mV * 4. */
    converted = (uint64_t)adc_voltage_mv * BOARD_MEASURE_IN_CURRENT_MA_PER_MV +
                0U;
    if(converted > UINT32_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_value = (uint32_t)converted;
    return PLATFORM_ERR_OK;
}
static platform_err_t bsp_measure_output_current_ma(uint16_t  raw,
                                                    uint32_t  reference_mv,
                                                    uint32_t *p_value)
{
    uint32_t       adc_voltage_mv = 0U;
    uint64_t       converted      = 0U;
    platform_err_t status         = PLATFORM_ERR_OK;

    if(NULL == p_value)
    {
        return PLATFORM_ERR_PARAM;
    }
    status = bsp_measure_adc_voltage_mv(raw, reference_mv, &adc_voltage_mv);
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    if(0U == adc_voltage_mv)
    {
        *p_value = 0U;
        return PLATFORM_ERR_OK;
    }
    /* Output INA180A2 gain 50, shunt 5 mohm: current mA = pin mV * 4. */
    converted = (uint64_t)adc_voltage_mv * BOARD_MEASURE_OUT_CURRENT_MA_PER_MV +
                0U;
    if(converted > UINT32_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_value = (uint32_t)converted;
    return PLATFORM_ERR_OK;
}
static platform_err_t bsp_measure_vdda_estimate_mv(uint16_t  raw,
                                                   uint32_t *p_value)
{
    if(NULL == p_value || raw > BOARD_MEASURE_ADC_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(0U == raw)
    {
        return PLATFORM_ERR_HW;
    }
    *p_value = (BOARD_MEASURE_VREFINT_NOMINAL_MV * BOARD_MEASURE_ADC_LEVELS +
                raw / 2U) /
               raw;
    return PLATFORM_ERR_OK;
}

/* Rntc = Rpullup * raw / (4096 - raw); VDDA cancels in this divider. */
static void convert_temperature(uint16_t                      raw,
                                bsp_measure_internal_value_t *p_value)
{
    float resistance;
    float inverse_kelvin;
    float temperature;
    if(NULL == p_value)
    {
        return;
    }
    p_value->temperature_deci_c = BSP_MEASURE_TEMPERATURE_INVALID;
    /* Guard log(0) and the divider denominator, without fault classification.
     */
    if(0U == raw || raw >= BOARD_MEASURE_ADC_MAX)
    {
        return;
    }
    resistance     = BOARD_MEASURE_NTC_PULLUP_OHM * (float)raw /
                     (float)(BOARD_MEASURE_ADC_LEVELS - raw);
    inverse_kelvin = 1.0f / BOARD_MEASURE_NTC_T25_K +
                     logf(resistance / BOARD_MEASURE_NTC_R25_OHM) /
                         BOARD_MEASURE_NTC_BETA_K;
    if(!(inverse_kelvin > 0.0f))
    {
        return;
    }
    temperature = (1.0f / inverse_kelvin - 273.15f) * 10.0f;
    if(!(temperature >= -32767.0f && temperature <= 32766.0f))
    {
        return;
    }
    p_value->temperature_deci_c = (int16_t)(temperature + ((temperature >= 0.0f)
                                                               ? 0.5f
                                                               : -0.5f));
}

static platform_err_t bsp_measure_convert(const uint16_t               *p_raw,
                                          size_t                        count,
                                          bsp_measure_internal_value_t *p_value)
{
    size_t                       index        = 0U;
    uint32_t                     reference_mv = 0U;
    bsp_measure_internal_value_t value        = {0};

    if(NULL == p_raw || NULL == p_value || BSP_MEASURE_CHANNEL_COUNT != count)
    {
        return PLATFORM_ERR_PARAM;
    }
    for(index = 0U; index < count; index++)
    {
        if(p_raw[index] > BOARD_MEASURE_ADC_MAX)
        {
            return PLATFORM_ERR_PARAM;
        }
    }
    if(0U == p_raw[BSP_MEASURE_VREFINT])
    {
        return PLATFORM_ERR_HW;
    }
    if(PLATFORM_ERR_OK !=
       bsp_measure_vdda_estimate_mv(p_raw[BSP_MEASURE_VREFINT], &reference_mv))
    {
        return PLATFORM_ERR_HW;
    }
    if(PLATFORM_ERR_OK !=
           bsp_measure_input_voltage_mv(p_raw[BSP_MEASURE_VIN_VOL],
                                        reference_mv,
                                        &value.input_voltage_mv) ||
       PLATFORM_ERR_OK !=
           bsp_measure_battery_voltage_mv(p_raw[BSP_MEASURE_BAT_VOL],
                                          reference_mv,
                                          &value.battery_voltage_mv) ||
       PLATFORM_ERR_OK !=
           bsp_measure_input_current_ma(p_raw[BSP_MEASURE_IN_CUR], reference_mv,
                                        &value.input_current_ma) ||
       PLATFORM_ERR_OK !=
           bsp_measure_output_current_ma(p_raw[BSP_MEASURE_OUT_CUR],
                                         reference_mv,
                                         &value.output_current_ma))
    {
        return PLATFORM_ERR_PARAM;
    }
    convert_temperature(p_raw[BSP_MEASURE_NTC], &value);
    *p_value = value;
    return PLATFORM_ERR_OK;
}

/* exported functions -------------------------------------------------------*/
void bsp_measure_set_callbacks(bsp_measure_data_callback_t  pf_data,
                               bsp_measure_error_callback_t pf_error)
{
    pf_measure_data  = pf_data;
    pf_measure_error = pf_error;
    g_last_error     = BSP_MEASURE_ERROR_NONE;
}

platform_err_t bsp_measure_init(void)
{
    if(FILTER_OK != filter_init(&g_input_voltage_filter,
                                MEASURE_VOLTAGE_FILTER_FRAMES) ||
       FILTER_OK != filter_init(&g_battery_voltage_filter,
                                MEASURE_VOLTAGE_FILTER_FRAMES) ||
       FILTER_OK != filter_init(&g_input_current_filter,
                                MEASURE_CURRENT_FILTER_FRAMES) ||
       FILTER_OK !=
           filter_init(&g_output_current_filter, MEASURE_CURRENT_FILTER_FRAMES))
    {
        g_measure_status = PLATFORM_ERR_HW;
        return g_measure_status;
    }
    /* ADC/DMA initialized, sampling trigger stopped; call once at startup. */
    g_measure_status      = plat_adc_start(BOARD_ADC_MEASURE, g_adc_dma_buffer,
                                           sizeof(g_adc_dma_buffer) /
                                               sizeof(g_adc_dma_buffer[0U]),
                                           measure_adc_callback);
    g_measure_initialized = (PLATFORM_ERR_OK == g_measure_status);
    return g_measure_status;
}

void bsp_measure_process(void)
{
    uint16_t                     raw[BSP_MEASURE_CHANNEL_COUNT] = {0U};
    uint32_t                     sequence = g_adc_frame_count;
    uint32_t                     index;
    bsp_measure_internal_value_t value = {0};
    bsp_measure_value_t          result;
    platform_err_t               status;
    bsp_measure_error_t          error = BSP_MEASURE_ERROR_NONE;

    if(0U == g_measure_initialized)
    {
        report_error(BSP_MEASURE_ERROR_INIT,
                     (PLATFORM_ERR_OK == g_measure_status) ? PLATFORM_ERR_HW
                                                           : g_measure_status);
        return;
    }
    if(0U != g_adc_dma_error_count)
    {
        status = reset_measure_filter();
        report_error((PLATFORM_ERR_OK == status) ? BSP_MEASURE_ERROR_DMA
                                                 : BSP_MEASURE_ERROR_FILTER,
                     PLATFORM_ERR_HW);
        return;
    }
    if(g_consumed_overrun_count != g_adc_overrun_count)
    {
        g_consumed_overrun_count = g_adc_overrun_count;
        g_consumed_sequence      = sequence;
        if(PLATFORM_ERR_OK != reset_measure_filter())
        {
            report_error(BSP_MEASURE_ERROR_FILTER, PLATFORM_ERR_HW);
        }
        return;
    }
    if(sequence == g_consumed_sequence)
    {
        return;
    }
    plat_adc_memory_barrier();
    for(index = 0U; index < BSP_MEASURE_CHANNEL_COUNT; index++)
    {
        raw[index] = g_adc_raw[index];
    }
    plat_adc_memory_barrier();
    if(sequence != g_adc_frame_count || 0U != g_adc_dma_error_count ||
       g_consumed_overrun_count != g_adc_overrun_count)
    {
        return;
    }
    if(1U != sequence - g_consumed_sequence &&
       PLATFORM_ERR_OK != reset_measure_filter())
    {
        report_error(BSP_MEASURE_ERROR_FILTER, PLATFORM_ERR_HW);
        return;
    }
    g_consumed_sequence = sequence;
    status = bsp_measure_convert(raw, BSP_MEASURE_CHANNEL_COUNT, &value);
    if(PLATFORM_ERR_OK != status)
    {
        error = BSP_MEASURE_ERROR_CONVERT;
    }
    else
    {
        status = filter_measure(&value);
        if(PLATFORM_ERR_OK != status)
        {
            error = BSP_MEASURE_ERROR_FILTER;
        }
    }
    if(BSP_MEASURE_ERROR_NONE != error)
    {
        if(PLATFORM_ERR_OK != reset_measure_filter())
        {
            error  = BSP_MEASURE_ERROR_FILTER;
            status = PLATFORM_ERR_HW;
        }
        report_error(error, status);
        return;
    }
    g_measure_status = PLATFORM_ERR_OK;
    g_last_error     = BSP_MEASURE_ERROR_NONE;
    if(NULL != pf_measure_data)
    {
        result.input_voltage_mv   = value.input_voltage_mv;
        result.battery_voltage_mv = value.battery_voltage_mv;
        result.input_current_ma   = value.input_current_ma;
        result.output_current_ma  = value.output_current_ma;
        result.battery_current_ma = (int32_t)value.input_current_ma -
                                    (int32_t)value.output_current_ma;
        result.temperature_deci_c = value.temperature_deci_c;
        pf_measure_data(&result, sequence);
    }
}
/* end of file --------------------------------------------------------------*/
