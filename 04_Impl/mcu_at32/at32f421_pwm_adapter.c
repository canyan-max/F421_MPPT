/**
 ******************************************************************************
 *@file               :   at32f421_pwm_adapter.c
 *@brief              :   Wrap TMR1 CH3 complementary PWM operations.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* includes -----------------------------------------------------------------*/
#include "at32f421_wk_config.h"
#include "plat_pwm.h"
/* define -------------------------------------------------------------------*/
/* typedef ------------------------------------------------------------------*/
/* variables ----------------------------------------------------------------*/
/* private functions --------------------------------------------------------*/
static void configure_pins(uint8_t enabled)
{
    gpio_init_type config;
    gpio_default_para_init(&config);
    config.gpio_mode     = (0U != enabled) ? GPIO_MODE_MUX : GPIO_MODE_OUTPUT;
    config.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    config.gpio_pull     = GPIO_PULL_NONE;
    config.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    if(0U != enabled)
    {
        gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE10, GPIO_MUX_2);
        gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE1, GPIO_MUX_2);
    }
    else
    {
        gpio_bits_reset(GPIOA, GPIO_PINS_10);
        gpio_bits_reset(GPIOB, GPIO_PINS_1);
    }
    config.gpio_pins = GPIO_PINS_10;
    gpio_init(GPIOA, &config);
    config.gpio_pins = GPIO_PINS_1;
    gpio_init(GPIOB, &config);
}
/* exported functions -------------------------------------------------------*/
platform_err_t plat_pwm_set_duty_permille(uint16_t duty_permille)
{
    uint32_t compare;
    compare = ((TMR1->pr + 1U) * duty_permille + 500U) / 1000U;
    tmr_channel_value_set(TMR1, TMR_SELECT_CHANNEL_3, compare);
    return PLATFORM_ERR_OK;
}

platform_err_t plat_pwm_start(uint16_t duty_permille)
{
    tmr_output_enable(TMR1, FALSE);
    tmr_counter_enable(TMR1, FALSE);
    /* Setter only writes compare and always returns OK. */
    plat_pwm_set_duty_permille(duty_permille);
    tmr_event_sw_trigger(TMR1, TMR_OVERFLOW_SWTRIG);
    tmr_counter_value_set(TMR1, 0U);
    configure_pins(1U);
    tmr_counter_enable(TMR1, TRUE);
    tmr_output_enable(TMR1, TRUE);
    return PLATFORM_ERR_OK;
}

platform_err_t plat_pwm_outputs_off(void)
{
    tmr_output_enable(TMR1, FALSE);
    configure_pins(0U);
    return PLATFORM_ERR_OK;
}
/* end of file --------------------------------------------------------------*/
