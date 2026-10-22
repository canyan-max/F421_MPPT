/**
 ******************************************************************************
 *@file               :   at32f421_gpio_adapter.c
 *@brief              :   Adapt logical GPIO resources to AT32F421 board pin
 *                        bindings.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include <stddef.h>
#include "at32f421_wk_config.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/
typedef struct
{
    gpio_type *p_port;
    uint16_t   pin;
    uint8_t    output;
} gpio_binding_t;

/* variables ----------------------------------------------------------------*/
/* Bind to generated names; do not duplicate initialization or guess polarity.
 */
static const gpio_binding_t g_bindings[BOARD_GPIO_RESOURCE_NUM] =
    {{LED1_PB3_GPIO_PORT, LED1_PB3_PIN, 1U},
     {LED2_PB4_GPIO_PORT, LED2_PB4_PIN, 1U},
     {LED3_PB5_GPIO_PORT, LED3_PB5_PIN, 1U},
     {SOL_MOS_OUT_PB12_GPIO_PORT, SOL_MOS_OUT_PB12_PIN, 1U},
     {LOADMOS_OUT_PB11_GPIO_PORT, LOADMOS_OUT_PB11_PIN, 1U},
     {BT_VDD_EN_PB10_GPIO_PORT, BT_VDD_EN_PB10_PIN, 1U},
     {RS485_TXEN_PF7_GPIO_PORT, RS485_TXEN_PF7_PIN, 1U},
     {OUT_CUR_ERR_GPIO_PORT, OUT_CUR_ERR_PIN, 0U}};
/* Only the presently configured PB0/EXINT0 is IRQ-capable. */
static plat_gpio_irq_callback_t pf_load_fault_irq;

/* private  functions  ------------------------------------------------------*/

/* exported functions -------------------------------------------------------*/
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM || 0U == g_bindings[id].output ||
       (PLAT_GPIO_RESET != state && PLAT_GPIO_SET != state))
    {
        return PLATFORM_ERR_PARAM;
    }
    gpio_bits_write(g_bindings[id].p_port, g_bindings[id].pin,
                    PLAT_GPIO_SET == state ? TRUE : FALSE);
    return PLATFORM_ERR_OK;
}

platform_err_t plat_gpio_read(plat_gpio_id_t id, plat_gpio_state_t *p_state)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM || NULL == p_state)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_state = RESET == gpio_input_data_bit_read(g_bindings[id].p_port,
                                                 g_bindings[id].pin)
                   ? PLAT_GPIO_RESET
                   : PLAT_GPIO_SET;
    return PLATFORM_ERR_OK;
}

platform_err_t plat_gpio_toggle(plat_gpio_id_t id)
{
    if(id >= BOARD_GPIO_RESOURCE_NUM || 0U == g_bindings[id].output)
    {
        return PLATFORM_ERR_PARAM;
    }
    gpio_bits_toggle(g_bindings[id].p_port, g_bindings[id].pin);
    return PLATFORM_ERR_OK;
}

platform_err_t plat_gpio_irq_bind(plat_gpio_id_t           id,
                                  plat_gpio_irq_callback_t pf_callback)
{
    uint32_t       mask;
    platform_err_t status = PLATFORM_ERR_OK;

    if(BOARD_GPIO_LOAD_FAULT != id || NULL == pf_callback)
    {
        return PLATFORM_ERR_PARAM;
    }
    mask = plat_irq_save();
    if(NULL == pf_load_fault_irq)
    {
        if(0U != (EXINT->inten & EXINT_LINE_0))
        {
            status = PLATFORM_ERR_BUSY;
        }
        else
        {
            pf_load_fault_irq = pf_callback;
        }
    }
    else if(pf_callback != pf_load_fault_irq)
    {
        status = PLATFORM_ERR_BUSY;
    }
    plat_irq_restore(mask);
    return status;
}

platform_err_t plat_gpio_irq_get_pending(plat_gpio_id_t id, uint8_t *p_pending)
{
    if(BOARD_GPIO_LOAD_FAULT != id || NULL == p_pending)
    {
        return PLATFORM_ERR_PARAM;
    }
    *p_pending = RESET != exint_flag_get(EXINT_LINE_0);
    return PLATFORM_ERR_OK;
}

uint8_t plat_gpio_irq_is_ready(plat_gpio_id_t id)
{
    return BOARD_GPIO_LOAD_FAULT == id && NULL != pf_load_fault_irq &&
           0U != (EXINT->inten & EXINT_LINE_0);
}

void plat_gpio_irq_handler(plat_gpio_id_t id)
{
    plat_gpio_irq_callback_t pf_callback;

    if(BOARD_GPIO_LOAD_FAULT != id ||
       RESET == exint_interrupt_flag_get(EXINT_LINE_0))
    {
        return;
    }
    pf_callback = pf_load_fault_irq;
    if(NULL != pf_callback)
    {
        pf_callback(id);
    }
    else
    {
        /* No callback may be installed for the first time on an armed line.
         * Leave it disabled so readiness cannot permit a load enable. */
        exint_interrupt_enable(EXINT_LINE_0, FALSE);
    }
    exint_flag_clear(EXINT_LINE_0);
}

/* end of file --------------------------------------------------------------*/
