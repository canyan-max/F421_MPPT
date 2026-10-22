param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('mppt-control-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include "svc_charge_mppt.c"

static svc_measure_value_t g_test_value;
static uint16_t g_test_duty;
static void start_test(uint16_t duty, uint32_t voltage)
{
    svc_charge_mppt_reset();
    g_test_duty = duty;
    g_test_value.battery_voltage_mv = voltage;
    g_test_value.input_voltage_mv = 18000U;
}
static void window(uint32_t current)
{
    unsigned int i;
    svc_charge_mppt_result_t result;
    g_test_value.input_current_ma = current;
    for(i = 0U; i < 50U; i++)
    {
        assert(PLATFORM_ERR_OK == svc_charge_mppt_update(
            &g_test_value, g_test_duty, &result));
        assert(result.duty_permille >= 150U && result.duty_permille <= 950U);
        assert((49U == i) == (0U != result.duty_changed));
        g_test_duty = result.duty_permille;
    }
}
int main(void)
{
    unsigned int i, direction, expected;
    svc_charge_mppt_result_t result;
    start_test(842U, 14000U);
    window(2000U);
    window(1996U);
    assert(0 == g_charge_mppt_power_change);
    window(1992U);
    assert(-1 == g_charge_mppt_power_change);
    assert(0U == g_charge_mppt_direction_increase);
    assert(843U == g_test_duty);
    for(direction = 0U; direction < 2U; direction++)
    {
        start_test(800U, 14000U);
        g_charge_mppt_direction_increase = (uint8_t)direction;
        window(2000U);
        window(2010U);
        assert(1 == g_charge_mppt_power_change);
        assert(direction == g_charge_mppt_direction_increase);
        window(2000U);
        assert(-1 == g_charge_mppt_power_change);
        assert((1U - direction) == g_charge_mppt_direction_increase);
    }
    start_test(800U, 10000U);
    window(2000U);
    window(1990U);
    assert(0 == g_charge_mppt_power_change);
    window(1989U);
    assert(-1 == g_charge_mppt_power_change);
    start_test(800U, 10000U);
    window(2000U);
    for(i = 1U; i <= 8U; i++) { window(2000U - i); }
    assert(0U == g_charge_mppt_trend_compare_count);
    assert(19920000U == g_charge_mppt_trend_reference_power_uw);
    svc_charge_mppt_pause(g_test_duty);
    assert(0U == g_charge_mppt_previous_power_valid);
    window(1800U);
    assert(18000000U == g_charge_mppt_trend_reference_power_uw);
    g_test_duty += 5U;
    window(1700U);
    assert(17000000U == g_charge_mppt_trend_reference_power_uw);
    for(i = 150U; i <= 950U; i++)
    {
        for(direction = 0U; direction < 2U; direction++)
        {
            start_test((uint16_t)i, 14000U);
            g_charge_mppt_direction_increase = (uint8_t)direction;
            expected = (150U == i || (950U != i && 0U != direction))
                ? i + 1U : i - 1U;
            window(2000U);
            assert(expected == g_test_duty);
        }
    }
    start_test(949U, 14000U);
    for(i = 0U; i < 2000U; i++) { window(2000U); }
    assert(g_charge_mppt_trend_compare_count < 8U);
    assert(PLATFORM_ERR_PARAM == svc_charge_mppt_update(NULL, 800U, &result));
    assert(PLATFORM_ERR_PARAM == svc_charge_mppt_update(&g_test_value, 149U, &result));
    assert(PLATFORM_ERR_PARAM == svc_charge_mppt_update(&g_test_value, 951U, &result));
    puts("PASS: MPPT directions, cumulative threshold, bounded reference, timing,");
    puts("      pause/rebaseline, all 801 duties both ways, long run, parameters");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $executable = Join-Path $testDirectory 'control.exe'
    $source | & $Gcc -x c -std=c99 -Wall -Wextra -Werror -I02_Service `
        -I03_Platform/bsp -I03_Platform/comm -I03_Platform/mcu_interface `
        -o $executable -
    if ($LASTEXITCODE -ne 0) { throw 'MPPT host compile failed' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'MPPT assertions failed' }
    Write-Output "Host executable retained at $testDirectory"
} finally { Pop-Location }
