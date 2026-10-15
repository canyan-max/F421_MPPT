/**
 ******************************************************************************
 *@file               :   svc_measure.c
 *@brief              :   Provide application access to board measurements.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "svc_measure.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
bsp_measure_value_t g_measure_value;
platform_err_t      g_measure_status = PLATFORM_ERR_BUSY;
static uint32_t     g_measure_sequence;
static uint16_t     g_measure_valid_count;
static uint8_t      g_measure_has_sequence;
/* private functions --------------------------------------------------------*/
static void measure_data_ready(const bsp_measure_value_t *p_value,
                               uint32_t                   sequence)
{
    (void)sequence;
    if(NULL == p_value)
    {
        g_measure_status      = PLATFORM_ERR_PARAM;
        g_measure_valid_count = 0U;
        return;
    }
    if(0U != g_measure_has_sequence &&
       1U != (uint32_t)(sequence - g_measure_sequence))
    {
        g_measure_valid_count = 0U;
    }
    g_measure_sequence     = sequence;
    g_measure_has_sequence = 1U;
    if(g_measure_valid_count < SVC_MEASURE_READY_FRAME_COUNT)
    {
        g_measure_valid_count++;
    }
    g_measure_value  = *p_value;
    g_measure_status = PLATFORM_ERR_OK;
}

static void measure_error(bsp_measure_error_t error, platform_err_t status)
{
    (void)error;
    g_measure_status      = status;
    g_measure_valid_count = 0U;
}
/* exported functions -------------------------------------------------------*/
platform_err_t svc_measure_init(void)
{
    platform_err_t status;

    g_measure_valid_count  = 0U;
    g_measure_has_sequence = 0U;
    bsp_measure_set_callbacks(measure_data_ready, measure_error);
    status           = bsp_measure_init();
    g_measure_status = (PLATFORM_ERR_OK == status) ? PLATFORM_ERR_BUSY : status;
    return status;
}

void svc_measure_process(void)
{
    bsp_measure_process();
}

platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    if(NULL == p_value)
    {
        return PLATFORM_ERR_PARAM;
    }
    if(PLATFORM_ERR_OK != g_measure_status)
    {
        return g_measure_status;
    }
    *p_value = g_measure_value;
    return PLATFORM_ERR_OK;
}

uint8_t svc_measure_is_ready(void)
{
    return (g_measure_valid_count >= SVC_MEASURE_READY_FRAME_COUNT) ? 1U : 0U;
}

uint32_t svc_measure_get_sequence(void)
{
    return g_measure_sequence;
}
/* end of file --------------------------------------------------------------*/
