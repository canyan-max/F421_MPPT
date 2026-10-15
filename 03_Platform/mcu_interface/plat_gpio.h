/**
 ******************************************************************************
 *@file               :   plat_gpio.h
 *@brief              :   Provide logical GPIO level read, write and toggle
 *                        interfaces.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef PLAT_GPIO_H
#define PLAT_GPIO_H

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"
#include "plat_resource.h"
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
/* Raw electrical levels, not LED/MOS logical on/off. Call after wk_gpio_config.
 * Only generated output pins are exposed. No EXINT API until configured. */
typedef enum PLAT_GPIO_STATE_T
{
    PLAT_GPIO_RESET = 0U,
    PLAT_GPIO_SET   = 1U,
} plat_gpio_state_t;

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state);
platform_err_t plat_gpio_read(plat_gpio_id_t id, plat_gpio_state_t *p_state);
/* Toggle is a non-atomic latch RMW: serialize writers to the same GPIO port;
 * read returns actual pin level. */
platform_err_t plat_gpio_toggle(plat_gpio_id_t id);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PLAT_GPIO_H */
/* end of file --------------------------------------------------------------*/
