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
 * Includes output resources and the generated PB0 fault input. */
typedef enum PLAT_GPIO_STATE_T
{
    PLAT_GPIO_RESET = 0U,
    PLAT_GPIO_SET   = 1U,
} plat_gpio_state_t;
/* Synchronous ISR callback: no blocking, logging or deferred fast shutdown. */
typedef void (*plat_gpio_irq_callback_t)(plat_gpio_id_t id);

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state);
platform_err_t plat_gpio_read(plat_gpio_id_t id, plat_gpio_state_t *p_state);
/* Toggle is a non-atomic latch RMW: serialize writers to the same GPIO port;
 * read returns actual pin level. */
platform_err_t plat_gpio_toggle(plat_gpio_id_t id);
/* Main context, before generated EXINT enable. Only configured IRQ-capable
 * IDs are accepted. NULL, replacement and late first binding are rejected;
 * rebinding the same callback is idempotent and does not clear pending flags.
 */
platform_err_t plat_gpio_irq_bind(plat_gpio_id_t           id,
                                  plat_gpio_irq_callback_t pf_callback);
platform_err_t plat_gpio_irq_get_pending(plat_gpio_id_t id, uint8_t *p_pending);
/* Zero for invalid ID, absent callback or disabled interrupt line. */
uint8_t plat_gpio_irq_is_ready(plat_gpio_id_t id);
/* IRQ entry dispatches its configured ID here. Callback runs before ACK.
 * Unbound events disable the line and ACK; they can never authorize output. */
void plat_gpio_irq_handler(plat_gpio_id_t id);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* PLAT_GPIO_H */
/* end of file --------------------------------------------------------------*/
