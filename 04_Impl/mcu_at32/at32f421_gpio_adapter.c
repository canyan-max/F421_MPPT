#include <stddef.h>
#include "at32f421_wk_config.h"
#include "board_resources.h"
#include "plat_gpio.h"
typedef struct { gpio_type *port; uint16_t pin; } gpio_binding_t;
/* Bind to generated names; do not duplicate initialization or guess polarity. */
static const gpio_binding_t bindings[BOARD_GPIO_RESOURCE_NUM] = {
    {LED1_PB3_GPIO_PORT, LED1_PB3_PIN},
    {LED2_PB4_GPIO_PORT, LED2_PB4_PIN},
    {LED3_PB5_GPIO_PORT, LED3_PB5_PIN},
    {SOL_MOS_OUT_PB12_GPIO_PORT, SOL_MOS_OUT_PB12_PIN},
    {LOADMOS_OUT_PB11_GPIO_PORT, LOADMOS_OUT_PB11_PIN},
    {BT_VDD_EN_PB10_GPIO_PORT, BT_VDD_EN_PB10_PIN},
    {RS485_TXEN_PF7_GPIO_PORT, RS485_TXEN_PF7_PIN}
};
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM || (state != PLAT_GPIO_RESET && state != PLAT_GPIO_SET))
        return PLATFORM_ERR_PARAM;
    gpio_bits_write(bindings[id].port, bindings[id].pin, state == PLAT_GPIO_SET ? TRUE : FALSE);
    return PLATFORM_ERR_OK;
}
platform_err_t plat_gpio_read(plat_gpio_id_t id, plat_gpio_state_t *state)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM || state == NULL) return PLATFORM_ERR_PARAM;
    *state = gpio_input_data_bit_read(bindings[id].port, bindings[id].pin) == RESET ? PLAT_GPIO_RESET : PLAT_GPIO_SET;
    return PLATFORM_ERR_OK;
}
platform_err_t plat_gpio_toggle(plat_gpio_id_t id)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM) return PLATFORM_ERR_PARAM;
    gpio_bits_toggle(bindings[id].port, bindings[id].pin);
    return PLATFORM_ERR_OK;
}
