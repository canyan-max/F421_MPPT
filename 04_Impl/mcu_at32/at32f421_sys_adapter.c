/**
 ******************************************************************************
 *@file               :   at32f435_sys.c
 *@brief              :   Provide the HAL APIs of description.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!  2026
 ******************************************************************************
 */

/* Includes -----------------------------------------------------------------*/
#include <stdint.h> /* stdint lib header file. */
#include "plat_sys.h"
#include "wk_system.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
uint32_t plat_irq_save(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

void plat_irq_restore(uint32_t mask)
{
    __set_PRIMASK(mask);
}

uint32_t plat_tick_get_ms(void)
{
    return (uint32_t)(wk_timebase_get());
}

void plat_delay_ms(uint32_t ms)
{
    if(0U == ms)
        return;

    wk_delay_ms(ms);
}
/* end of  file -------------------------------------------------------------*/
