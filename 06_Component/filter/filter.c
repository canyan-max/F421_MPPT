/*
 * ****************************************************************************
 * Copyright (C), Inc.(Gmbh) or its affiliates.
 * All Rights Reserved.
 * 
 * @file               :   filter.c
 * 
 * @pardependencies    :
 *                         filter.c
 *                         xxx.h   
 *                         xxxx.h
 * 
 * @author             :   null
 * 
 * @brief              :   Provide the HAL APIs of .
 * 
 * @Processingflow     :
 * 
 * @calldirectly       :
 * 
 * @version            :   V1.0 
 * 
 * @note               :   1 tab == 4 spaces!
 * ****************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "filter.h"
/* define -------------------------------------------------------------------*/
/* typedef ------------------------------------------------------------------*/
/* variables ----------------------------------------------------------------*/
/* private functions --------------------------------------------------------*/
static uint8_t state_valid(const MovingAvg_t *p_filter)
{
    int64_t minimum;
    int64_t maximum;
    if(NULL == p_filter || 0U == p_filter->size ||
       p_filter->size > FILTER_MAX_WINDOW_SIZE ||
       p_filter->count > p_filter->size || p_filter->index >= p_filter->size ||
       (p_filter->count < p_filter->size && p_filter->index != p_filter->count))
    {
        return 0U;
    }
    minimum = (int64_t)INT32_MIN * p_filter->count;
    maximum = (int64_t)INT32_MAX * p_filter->count;
    return (p_filter->sum >= minimum && p_filter->sum <= maximum);
}
/* exported functions -------------------------------------------------------*/
filter_status_t filter_init(MovingAvg_t *p_filter, uint8_t window_size)
{
    uint8_t index;
    if(NULL == p_filter)
    {
        return FILTER_ERR_PARAM;
    }
    p_filter->size  = 0U;
    p_filter->index = 0U;
    p_filter->count = 0U;
    p_filter->sum   = 0;
    for(index = 0U; index < FILTER_MAX_WINDOW_SIZE; index++)
    {
        p_filter->buffer[index] = 0;
    }
    if(0U == window_size || window_size > FILTER_MAX_WINDOW_SIZE)
    {
        return FILTER_ERR_PARAM;
    }
    p_filter->size = window_size;
    return FILTER_OK;
}

filter_status_t
filter_add(MovingAvg_t *p_filter, int32_t sample, int32_t *p_average)
{
    if(NULL == p_filter || NULL == p_average)
    {
        return FILTER_ERR_PARAM;
    }
    if(0U == state_valid(p_filter))
    {
        return FILTER_ERR_STATE;
    }
    if(p_filter->count == p_filter->size)
    {
        p_filter->sum -= p_filter->buffer[p_filter->index];
    }
    else
    {
        p_filter->count++;
    }
    p_filter->buffer[p_filter->index] = sample;
    p_filter->sum += sample;
    p_filter->index++;
    if(p_filter->index == p_filter->size)
    {
        p_filter->index = 0U;
    }
    *p_average = (int32_t)(p_filter->sum / p_filter->count);
    return FILTER_OK;
}

filter_status_t filter_get_average(const MovingAvg_t *p_filter,
                                   int32_t           *p_average)
{
    if(NULL == p_filter || NULL == p_average)
    {
        return FILTER_ERR_PARAM;
    }
    if(0U == state_valid(p_filter))
    {
        return FILTER_ERR_STATE;
    }
    *p_average = (0U == p_filter->count)
                     ? 0
                     : (int32_t)(p_filter->sum / p_filter->count);
    return FILTER_OK;
}

filter_status_t filter_reset(MovingAvg_t *p_filter)
{
    uint8_t size;
    if(NULL == p_filter)
    {
        return FILTER_ERR_PARAM;
    }
    size = p_filter->size;
    /* Init validates size before it can be used as an array bound. */
    return filter_init(p_filter, size);
}
/* end of file --------------------------------------------------------------*/
