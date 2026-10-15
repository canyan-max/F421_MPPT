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
#include "board_resources.h"
/* define   -----------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* private  functions  ------------------------------------------------------*/

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
    return plat_gpio_write(BOARD_GPIO_LOAD_SWITCH,
                           (0U != enabled) ? PLAT_GPIO_SET : PLAT_GPIO_RESET);
}

/* end of file --------------------------------------------------------------*/
