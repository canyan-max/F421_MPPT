#ifndef PLAT_GPIO_H
#define PLAT_GPIO_H
#include "plat_error.h"
#include "plat_resource.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Raw electrical levels, not LED/MOS logical on/off. Call after wk_gpio_config.
 * Only generated output pins are exposed. No EXINT API until configured. */
typedef enum { PLAT_GPIO_RESET = 0, PLAT_GPIO_SET = 1 } plat_gpio_state_t;
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state);
platform_err_t plat_gpio_read(plat_gpio_id_t id, plat_gpio_state_t *state);
/* Toggle is a non-atomic latch RMW: serialize writers to the same GPIO port; read returns actual pin level. */
platform_err_t plat_gpio_toggle(plat_gpio_id_t id);
#ifdef __cplusplus
}
#endif
#endif
