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
    /* Coarse startup is opt-in; ordinary tests above retain 1 permille. */
    start_test(631U, 12500U);
    assert(SVC_CHARGE_MPPT_PHASE_OFF == svc_charge_mppt_get_phase());
    svc_charge_mppt_startup_begin();
    assert(SVC_CHARGE_MPPT_PHASE_STARTUP == svc_charge_mppt_get_phase());
    for(i = 0U; i < 12U; i++) { window(700U + 200U * i); }
    assert(691U == g_test_duty); /* 6 s instead of 30 s at fine cadence. */
    assert(0U != g_charge_mppt_startup_active);
    window(2800U); /* Last coarse step loses >0.1 W: undo 5 permille. */
    assert(686U == g_test_duty);
    assert(0U == g_charge_mppt_startup_active);
    assert(0U == g_charge_mppt_previous_power_valid);
    assert(SVC_CHARGE_MPPT_PHASE_TRACKING == svc_charge_mppt_get_phase());
    window(2900U);
    assert(687U == g_test_duty); /* Fine search builds a fresh baseline. */

    start_test(631U, 12500U);
    svc_charge_mppt_startup_begin();
    for(i = 0U; i < 20U; i++) { window(1000U); }
    assert(0U == g_charge_mppt_startup_active);
    assert(727U == g_test_duty); /* 19 coarse trials, then one fine step. */
    window(1000U);
    assert(728U == g_test_duty);

    start_test(631U, 12500U);
    svc_charge_mppt_startup_begin();
    window(1000U);
    svc_charge_mppt_pause(g_test_duty);
    assert(0U == g_charge_mppt_startup_active);
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());
    window(1000U);
    assert(SVC_CHARGE_MPPT_PHASE_TRACKING == svc_charge_mppt_get_phase());
    assert(637U == g_test_duty);
    svc_charge_mppt_startup_begin();
    window(1000U);
    g_test_duty++;
    window(1000U);
    assert(0U == g_charge_mppt_startup_active);
    svc_charge_mppt_startup_begin();
    svc_charge_mppt_reset();
    assert(0U == g_charge_mppt_startup_active);
    /* Coarse startup saturates safely at every allowed duty. */
    for(i = 150U; i <= 950U; i++)
    {
        start_test((uint16_t)i, 12500U);
        svc_charge_mppt_startup_begin();
        window(1000U);
        expected = (950U == i) ? 949U : ((i + 5U > 950U) ? 950U : i + 5U);
        assert(expected == g_test_duty);
        if(950U == g_test_duty)
        {
            window(1100U);
            assert(949U == g_test_duty);
            assert(0U == g_charge_mppt_startup_active);
        }
    }
    /* Resume must not reuse either old direction or the old power baseline. */
    for(direction = 0U; direction < 2U; direction++)
    {
        start_test(780U, 14000U);
        g_charge_mppt_direction_increase = (uint8_t)direction;
        window(3000U);
        svc_charge_mppt_pause(g_test_duty);
        g_test_duty = 800U; /* CC has moved the operating point. */
        svc_charge_mppt_resume(g_test_duty);
        assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
        assert(0U == g_charge_mppt_previous_power_valid);
        window(2000U); /* Fresh baseline, then UP trial independent of old D. */
        assert(801U == g_test_duty);
        assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
        window(1990U); /* Reject loss and restore baseline D. */
        assert(800U == g_test_duty);
        assert(0U == g_charge_mppt_previous_power_valid);
        window(2000U); /* Measure restored point before DOWN trial. */
        assert(799U == g_test_duty);
        window(2010U); /* Confirm gain at the lower neighbor. */
        assert(798U == g_test_duty);
        assert(SVC_CHARGE_MPPT_PHASE_TRACKING == svc_charge_mppt_get_phase());
        assert(0U == g_charge_mppt_direction_increase);
    }
    start_test(800U, 14000U);
    svc_charge_mppt_resume(g_test_duty);
    window(2000U);
    window(2010U); /* UP gains power: enter normal tracking. */
    assert(802U == g_test_duty);
    assert(SVC_CHARGE_MPPT_PHASE_TRACKING == svc_charge_mppt_get_phase());
    start_test(800U, 14000U);
    svc_charge_mppt_resume(g_test_duty);
    window(2000U); window(2000U); window(2000U); window(2000U);
    assert(800U == g_test_duty); /* Both inconclusive: bounded rollback. */
    assert(SVC_CHARGE_MPPT_PHASE_TRACKING == svc_charge_mppt_get_phase());
    for(i = 150U; i <= 950U; i++)
    {
        start_test((uint16_t)i, 14000U);
        svc_charge_mppt_resume(g_test_duty);
        window(2000U);
        assert(((950U == i) ? 949U : i + 1U) == g_test_duty);
        window(1990U);
        assert(i == g_test_duty);
    }
    start_test(800U, 14000U);
    svc_charge_mppt_resume(g_test_duty);
    window(2000U);
    svc_charge_mppt_pause(g_test_duty); /* Interrupted trial cannot survive. */
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());
    svc_charge_mppt_resume(810U);
    g_test_duty = 810U;
    window(1000U);
    assert(811U == g_test_duty);
    assert(14000000U == g_charge_mppt_previous_power_raw);
    svc_charge_mppt_reset();
    assert(SVC_CHARGE_MPPT_PHASE_OFF == svc_charge_mppt_get_phase());
    puts("PASS: fresh resume direction, two-neighbor verification/rollback,");
    puts("      bounded inconclusive trials, interruption and all duty boundaries");
    puts("PASS: bounded coarse startup, 5x progress, loss rollback/rebaseline,");
    puts("      10 s bound, CC pause, external duty cancellation, all boundaries");
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
