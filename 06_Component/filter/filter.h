/*
 * ****************************************************************************
 * Copyright (C), Inc.(Gmbh) or its affiliates.
 * All Rights Reserved.
 * 
 * @file               :   filter.h
 * 
 * @pardependencies    :
 *                         filter.h
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

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef FILTER_H
#define FILTER_H
/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
/* define -------------------------------------------------------------------*/
#define FILTER_MAX_WINDOW_SIZE (16U)
/* typedef ------------------------------------------------------------------*/
typedef enum FILTER_STATUS_T
{
    FILTER_OK = 0U,
    FILTER_ERR_PARAM,
    FILTER_ERR_STATE,
} filter_status_t;

typedef struct MOVING_AVG_T
{
    int32_t buffer[FILTER_MAX_WINDOW_SIZE];
    int64_t sum;
    uint8_t size;
    uint8_t index;
    uint8_t count;
} MovingAvg_t;
/* variables ----------------------------------------------------------------*/
/* function -----------------------------------------------------------------*/
/* Single-context owner, static allocation. Initialize before any other call;
 * zero initialization alone is not a valid filter. Do not mutate members.
 * Window must be 1..16. Invalid init clears the instance to disabled state.
 * Supports full int32 range; 16 samples safely fit the int64 sum.
 * Output must not overlap the instance. Errors leave output unchanged.
 * Warm-up uses available samples; average truncates toward zero.
 * Empty initialized filter has average zero. Reset preserves valid size. */
filter_status_t filter_init(MovingAvg_t *p_filter, uint8_t window_size);
filter_status_t
filter_add(MovingAvg_t *p_filter, int32_t sample, int32_t *p_average);
filter_status_t filter_get_average(const MovingAvg_t *p_filter,
                                   int32_t           *p_average);
filter_status_t filter_reset(MovingAvg_t *p_filter);
#ifdef __cplusplus
}
#endif
#endif /* FILTER_H */
/* end of file --------------------------------------------------------------*/
