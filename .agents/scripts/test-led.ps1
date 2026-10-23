param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('led-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "svc_led.h"
#include "bsp_led.h"
#include "svc_charge.h"
#include "svc_discharge.h"
#include "svc_measure.h"
#include "plat_gpio.h"
#include "board_resources.h"

volatile uint8_t g_discharge_manual_enabled;
uint8_t g_discharge_output_enabled;
svc_discharge_state_t g_discharge_state;
uint32_t g_discharge_fault_flags;
static svc_measure_value_t g_test_value;
static svc_discharge_config_t g_test_config;
static svc_charge_state_t g_test_charge_state;
static svc_charge_fault_t g_test_charge_fault;
static uint16_t g_test_duty;
static uint8_t g_test_ready;
static uint8_t g_test_null_config;
static platform_err_t g_test_measure_status;
static platform_err_t g_test_write_status[3U];
static plat_gpio_state_t g_test_level[3U];
static uint32_t g_test_writes;

platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    assert(NULL != p_value);
    *p_value = g_test_value;
    return g_test_measure_status;
}
uint8_t svc_measure_is_ready(void) { return g_test_ready; }
svc_charge_state_t svc_charge_get_state(void) { return g_test_charge_state; }
svc_charge_fault_t svc_charge_get_fault(void) { return g_test_charge_fault; }
uint16_t svc_charge_get_duty_permille(void) { return g_test_duty; }
const svc_discharge_config_t *svc_discharge_get_config(void)
{
    return g_test_null_config ? NULL : &g_test_config;
}
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state)
{
    /* No PWM, load GPIO or solar switch write can pass this stub. */
    assert(BOARD_GPIO_LED1 == id || BOARD_GPIO_LED2 == id || BOARD_GPIO_LED3 == id);
    assert(PLAT_GPIO_SET == state || PLAT_GPIO_RESET == state);
    g_test_writes++;
    if(PLATFORM_ERR_OK == g_test_write_status[id]) { g_test_level[id] = state; }
    return g_test_write_status[id];
}
static void expect_leds(unsigned int charge, unsigned int battery, unsigned int load)
{
    assert((charge ? PLAT_GPIO_RESET : PLAT_GPIO_SET) == g_test_level[BOARD_GPIO_LED3]);
    assert((battery ? PLAT_GPIO_RESET : PLAT_GPIO_SET) == g_test_level[BOARD_GPIO_LED1]);
    assert((load ? PLAT_GPIO_RESET : PLAT_GPIO_SET) == g_test_level[BOARD_GPIO_LED2]);
}
static platform_err_t process(void)
{
    uint32_t faults = g_discharge_fault_flags;
    uint8_t manual = g_discharge_manual_enabled;
    uint8_t output = g_discharge_output_enabled;
    svc_discharge_state_t state = g_discharge_state;
    svc_charge_state_t charge_state = g_test_charge_state;
    svc_charge_fault_t charge_fault = g_test_charge_fault;
    uint16_t duty = g_test_duty;
    platform_err_t status = svc_led_process();
    assert(faults == g_discharge_fault_flags && manual == g_discharge_manual_enabled);
    assert(output == g_discharge_output_enabled && state == g_discharge_state);
    assert(charge_state == g_test_charge_state && charge_fault == g_test_charge_fault);
    assert(duty == g_test_duty);
    return status;
}
static void normal(void)
{
    memset(&g_test_value, 0, sizeof(g_test_value));
    memset(&g_test_config, 0, sizeof(g_test_config));
    memset(g_test_write_status, 0, sizeof(g_test_write_status));
    g_test_value.battery_voltage_mv = 13000U;
    g_test_value.temperature_deci_c = 250;
    g_test_config.voltage_trip_mv = 10000U;
    g_test_ready = 1U;
    g_test_null_config = 0U;
    g_test_measure_status = PLATFORM_ERR_OK;
    g_test_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
    g_test_charge_fault = SVC_CHARGE_FAULT_NONE;
    g_test_duty = 800U;
    g_discharge_state = SVC_DISCHARGE_STATE_ON;
    g_discharge_output_enabled = 1U;
    g_discharge_manual_enabled = 1U;
    g_discharge_fault_flags = SVC_DISCHARGE_FAULT_NONE;
}
int main(void)
{
    unsigned int index;
    uint32_t writes;
    const uint32_t other_faults[] = {
        SVC_DISCHARGE_FAULT_TEMPERATURE_LOW,
        SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH,
        SVC_DISCHARGE_FAULT_OVERCURRENT,
        SVC_DISCHARGE_FAULT_GPIO,
        SVC_DISCHARGE_FAULT_SHORT_CIRCUIT
    };

    normal();
    assert(PLATFORM_ERR_OK == svc_led_init());
    expect_leds(0U, 0U, 0U);
    for(index = 0U; index < 8U; index++)
    {
        assert(PLATFORM_ERR_OK == bsp_led_set(index & 1U, (index >> 1U) & 1U,
                                             (index >> 2U) & 1U));
        expect_leds(index & 1U, (index >> 1U) & 1U, (index >> 2U) & 1U);
    }
    writes = g_test_writes;
    assert(PLATFORM_ERR_PARAM == bsp_led_set(2U, 0U, 0U));
    assert(PLATFORM_ERR_PARAM == bsp_led_set(0U, 2U, 0U));
    assert(PLATFORM_ERR_PARAM == bsp_led_set(0U, 0U, 2U));
    assert(writes == g_test_writes);

    for(index = SVC_CHARGE_STATE_OFF; index <= SVC_CHARGE_STATE_MPPT; index++)
    {
        unsigned int active = SVC_CHARGE_STATE_WAKE_CV == index ||
                              SVC_CHARGE_STATE_MPPT == index ||
                              SVC_CHARGE_STATE_CONSTANT_CURRENT == index ||
                              SVC_CHARGE_STATE_CONSTANT_VOLTAGE == index ||
                              SVC_CHARGE_STATE_MAINTENANCE == index;
        g_test_charge_state = (svc_charge_state_t)index;
        assert(PLATFORM_ERR_OK == process());
        expect_leds(active, 1U, 1U);
    }
    g_test_charge_state = (svc_charge_state_t)99;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(0U, 1U, 1U);
    normal();
    for(index = SVC_CHARGE_FAULT_MEASUREMENT; index <= SVC_CHARGE_FAULT_CONFIG; index++)
    {
        g_test_charge_fault = (svc_charge_fault_t)index;
        assert(PLATFORM_ERR_OK == process());
        expect_leds(0U, 1U, 1U);
    }
    normal();
    g_test_duty = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(0U, 1U, 1U);

    normal();
    g_test_measure_status = PLATFORM_ERR_HW;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(0U, 0U, 0U);
    normal();
    g_test_ready = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(0U, 0U, 0U);
    normal();
    g_test_value.temperature_deci_c = SVC_MEASURE_TEMPERATURE_INVALID;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(0U, 0U, 0U);

    normal();
    g_test_value.battery_voltage_mv = 10000U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 1U); /* Raw low battery is visible before its trip latches. */
    g_test_value.battery_voltage_mv = 11000U;
    g_discharge_fault_flags = SVC_DISCHARGE_FAULT_BATTERY_LOW;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 0U);
    g_test_value.battery_voltage_mv = 12000U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 0U); /* Do not bypass the discharge recovery confirmation. */
    g_discharge_fault_flags = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 1U, 1U);
    g_test_config.voltage_trip_mv = 20000U;
    g_test_value.battery_voltage_mv = 20000U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 1U);
    g_test_value.battery_voltage_mv = 24000U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 1U, 1U);

    normal();
    for(index = 0U; index < sizeof(other_faults) / sizeof(other_faults[0U]); index++)
    {
        g_discharge_fault_flags = other_faults[index];
        assert(PLATFORM_ERR_OK == process());
        expect_leds(1U, 1U, 0U);
    }
    g_discharge_fault_flags = SVC_DISCHARGE_FAULT_MEASUREMENT;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 0U);
    g_discharge_fault_flags = SVC_DISCHARGE_FAULT_CONFIG;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 0U);
    normal();
    g_discharge_manual_enabled = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 1U, 0U);
    normal();
    g_discharge_output_enabled = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 1U, 0U);
    normal();
    for(index = SVC_DISCHARGE_STATE_WAIT_SAMPLE; index <= SVC_DISCHARGE_STATE_FAULT; index++)
    {
        g_discharge_state = (svc_discharge_state_t)index;
        assert(PLATFORM_ERR_OK == process());
        expect_leds(1U, 1U, SVC_DISCHARGE_STATE_ON == index);
    }
    normal();
    g_test_null_config = 1U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 1U);
    normal();
    g_test_config.voltage_trip_mv = 0U;
    assert(PLATFORM_ERR_OK == process());
    expect_leds(1U, 0U, 1U);

    /* A failed LED write must not skip other lamps or change any power state. */
    normal();
    for(index = 0U; index < 3U; index++)
    {
        g_test_write_status[index] = PLATFORM_ERR_HW;
        writes = g_test_writes;
        assert(PLATFORM_ERR_HW == process());
        assert(writes + 3U == g_test_writes);
        g_test_write_status[index] = PLATFORM_ERR_OK;
        assert(PLATFORM_ERR_OK == process());
        expect_leds(1U, 1U, 1U);
    }
    puts("PASS: active-low PB5 charge / PB3 battery / PB4 load mapping, all states,");
    puts("      UV thresholds/latch recovery, sample errors, manual/fault independence,");
    puts("      GPIO errors retried, parameter checks, no power-control writes");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $executable = Join-Path $testDirectory 'test.exe'
    $source | & $Gcc -std=c99 -Wall -Wextra -Werror -I02_Service `
        -I00_Project_Config -I03_Platform/bsp -I03_Platform/comm `
        -I03_Platform/mcu_interface -x c - 02_Service/svc_led.c `
        03_Platform/bsp/bsp_led.c -o $executable
    if ($LASTEXITCODE -ne 0) { throw 'LED host compile failed' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'LED assertions failed' }
    Write-Output "Host executable retained at $testDirectory"
} finally { Pop-Location }
