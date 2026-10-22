/**
 ******************************************************************************
 *@file               :   bsp_power_path.c
 *@brief              :   Control board charging and discharging power paths.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "bsp_power_path.h"
#include "plat_pwm.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/
static volatile uint32_t       g_discharge_short_generation;
static volatile uint8_t        g_discharge_short_latched;
static volatile platform_err_t g_discharge_short_off_status;
static uint8_t                 g_discharge_interlock_ready;

/* private  functions  ------------------------------------------------------*/
static uint8_t discharge_short_active(void)
{
    plat_gpio_state_t state;
    platform_err_t    status = plat_gpio_read(BOARD_GPIO_LOAD_FAULT, &state);
    return PLATFORM_ERR_OK != status || PLAT_GPIO_RESET == state;
}

static uint8_t discharge_short_pending(void)
{
    uint8_t        pending;
    platform_err_t status = plat_gpio_irq_get_pending(BOARD_GPIO_LOAD_FAULT,
                                                      &pending);
    return PLATFORM_ERR_OK != status || 0U != pending;
}

static void latch_discharge_short(void)
{
    /* Used synchronously by IRQ callback and by atomic main-context checks. */
    g_discharge_short_off_status = plat_gpio_write(BOARD_GPIO_LOAD_SWITCH,
                                                   PLAT_GPIO_RESET);
    g_discharge_short_latched    = 1U;
    g_discharge_short_generation++;
}

static void discharge_short_callback(plat_gpio_id_t id)
{
    if(BOARD_GPIO_LOAD_FAULT == id)
    {
        latch_discharge_short();
    }
}

/* exported functions -------------------------------------------------------*/
platform_err_t bsp_power_path_charge_start(uint16_t duty_permille)
{
    platform_err_t status;

    if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    status = plat_pwm_outputs_off();
    if(PLATFORM_ERR_OK != status)
    {
        return status;
    }
    status = plat_gpio_write(BOARD_GPIO_SOL_SWITCH, PLAT_GPIO_SET);
    if(PLATFORM_ERR_OK == status)
    {
        status = plat_pwm_start(duty_permille);
    }
    if(PLATFORM_ERR_OK != status)
    {
        platform_err_t stop_status = bsp_power_path_charge_stop();
        return (PLATFORM_ERR_OK != stop_status) ? stop_status : status;
    }
    return PLATFORM_ERR_OK;
}

platform_err_t bsp_power_path_charge_set_duty(uint16_t duty_permille)
{
    if(BSP_POWER_PATH_CHARGE_DUTY_MIN_PERMILLE > duty_permille ||
       BSP_POWER_PATH_CHARGE_DUTY_MAX_PERMILLE < duty_permille)
    {
        return PLATFORM_ERR_PARAM;
    }
    return plat_pwm_set_duty_permille(duty_permille);
}

platform_err_t bsp_power_path_charge_stop(void)
{
    platform_err_t pwm_status  = plat_pwm_outputs_off();
    platform_err_t gpio_status = plat_gpio_write(BOARD_GPIO_SOL_SWITCH,
                                                 PLAT_GPIO_RESET);
    return (PLATFORM_ERR_OK != pwm_status) ? pwm_status : gpio_status;
}

platform_err_t bsp_power_path_discharge_set(uint8_t enabled)
{
    uint32_t       mask = plat_irq_save();
    platform_err_t status;

    if(0U != enabled &&
       (0U == g_discharge_interlock_ready ||
        0U == plat_gpio_irq_is_ready(BOARD_GPIO_LOAD_FAULT) ||
        0U != g_discharge_short_latched || 0U != discharge_short_active() ||
        0U != discharge_short_pending()))
    {
        if(0U != g_discharge_interlock_ready && 0U == g_discharge_short_latched)
        {
            /* Preserve a polled fault even if its level clears before ISR. */
            latch_discharge_short();
            status = g_discharge_short_off_status;
        }
        else
        {
            status = plat_gpio_write(BOARD_GPIO_LOAD_SWITCH, PLAT_GPIO_RESET);
        }
        if(PLATFORM_ERR_OK == status)
        {
            status = PLATFORM_ERR_BUSY;
        }
    }
    else
    {
        status = plat_gpio_write(BOARD_GPIO_LOAD_SWITCH, (0U != enabled)
                                                             ? PLAT_GPIO_SET
                                                             : PLAT_GPIO_RESET);
        /* An edge arriving while IRQs are masked must also veto this ON.
         * Hardware assertion after the final check is handled on IRQ restore.
         */
        if(PLATFORM_ERR_OK == status && 0U != enabled &&
           (0U != discharge_short_active() || 0U != discharge_short_pending()))
        {
            latch_discharge_short();
            status = g_discharge_short_off_status;
            if(PLATFORM_ERR_OK == status)
            {
                status = PLATFORM_ERR_BUSY;
            }
        }
    }
    plat_irq_restore(mask);
    return status;
}

platform_err_t bsp_power_path_discharge_init(void)
{
    platform_err_t status       = plat_gpio_irq_bind(BOARD_GPIO_LOAD_FAULT,
                                                     discharge_short_callback);
    g_discharge_interlock_ready = PLATFORM_ERR_OK == status;
    return status;
}

bsp_discharge_short_state_t bsp_power_path_discharge_short_get(void)
{
    bsp_discharge_short_state_t state;
    uint32_t                    mask = plat_irq_save();

    if(0U == g_discharge_short_latched &&
       (0U != discharge_short_active() || 0U != discharge_short_pending()))
    {
        latch_discharge_short();
    }
    state.generation = g_discharge_short_generation;
    state.latched    = g_discharge_short_latched;
    plat_irq_restore(mask);
    return state;
}

platform_err_t bsp_power_path_discharge_short_rearm(uint32_t generation)
{
    uint32_t       mask   = plat_irq_save();
    platform_err_t status = PLATFORM_ERR_BUSY;

    if(0U != g_discharge_interlock_ready &&
       0U != plat_gpio_irq_is_ready(BOARD_GPIO_LOAD_FAULT) &&
       generation == g_discharge_short_generation &&
       0U == discharge_short_active() && 0U == discharge_short_pending())
    {
        g_discharge_short_latched = 0U;
        status                    = PLATFORM_ERR_OK;
    }
    plat_irq_restore(mask);
    return status;
}

/* end of file --------------------------------------------------------------*/
