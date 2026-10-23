param(
    [string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe',
    [string]$BaselineDirectory
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('charge-split-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "svc_charge_cc.h"
#include "svc_charge_cv.h"
#include "svc_charge.c"
#include "svc_charge_mppt.c"

static uint8_t g_test_pwm_on;
static uint8_t g_test_ready = 1U;
static uint32_t g_test_sequence;
static uint32_t g_test_start_count;
static uint32_t g_test_stop_count;
static uint32_t g_test_set_count;
static uint32_t g_test_off_vin = 18000U;
static uint32_t g_test_on_vin = 16900U;
static uint32_t g_test_battery = 13700U;
static uint32_t g_test_current = 2700U;
static uint32_t g_test_load = 10U;
static platform_err_t g_test_measure_status = PLATFORM_ERR_OK;
static platform_err_t g_test_set_status = PLATFORM_ERR_OK;
static int16_t g_test_temperature = 250;
static platform_err_t g_test_stop_status = PLATFORM_ERR_OK;
static platform_err_t g_test_start_status = PLATFORM_ERR_OK;
static uint16_t g_test_initial_duty;

platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    assert(NULL != p_value);
    memset(p_value, 0, sizeof(*p_value));
    p_value->input_voltage_mv = g_test_pwm_on ? g_test_on_vin : g_test_off_vin;
    p_value->battery_voltage_mv = g_test_battery;
    p_value->input_current_ma = g_test_pwm_on ? g_test_current : 0U;
    p_value->output_current_ma = g_test_load;
    p_value->battery_current_ma = (int32_t)p_value->input_current_ma - (int32_t)g_test_load;
    p_value->temperature_deci_c = g_test_temperature;
    return g_test_measure_status;
}
uint32_t svc_measure_get_sequence(void) { return g_test_sequence; }
uint8_t svc_measure_is_ready(void) { return g_test_ready; }

platform_err_t bsp_power_path_charge_stop(void)
{
    g_test_stop_count++;
    if(PLATFORM_ERR_OK == g_test_stop_status) { g_test_pwm_on = 0U; }
    return g_test_stop_status;
}
platform_err_t bsp_power_path_charge_start(uint16_t duty)
{
    g_test_start_count++;
    g_test_initial_duty = duty;
    if(PLATFORM_ERR_OK == g_test_start_status) { g_test_pwm_on = 1U; }
    return g_test_start_status;
}
platform_err_t bsp_power_path_charge_set_duty(uint16_t duty)
{
    assert(g_test_pwm_on);
    assert(duty >= 150U && duty <= 950U);
    g_test_set_count++;
    return g_test_set_status;
}

static void tick(void)
{
    g_test_sequence += 10U;
    svc_charge_process();
}

extern uint32_t g_charge_full_confirm_tick_count;
extern uint8_t g_charge_full_confirmed;
extern uint32_t g_charge_maintenance_recharge_tick_count;
static void begin_tracking(void)
{
    unsigned int i;
    g_test_stop_status = PLATFORM_ERR_OK;
    g_test_start_status = PLATFORM_ERR_OK;
    g_test_set_status = PLATFORM_ERR_OK;
    g_test_measure_status = PLATFORM_ERR_OK;
    g_test_temperature = 250;
    g_test_battery = 13700U;
    g_test_current = 2700U;
    g_test_load = 10U;
    g_test_off_vin = 18000U;
    g_test_on_vin = 16900U;
    g_test_ready = 1U;
    assert(PLATFORM_ERR_OK == svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_12V));
    assert(PLATFORM_ERR_OK == svc_charge_start());
    for(i = 0U; i < 3U; i++) { tick(); }
}
int run_split_tests(void)
{
    unsigned int i;
    uint16_t duty;
    uint32_t writes;
    begin_tracking();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(SVC_CHARGE_MPPT_PHASE_STARTUP == svc_charge_mppt_get_phase());
    duty = g_charge_duty_permille;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(duty + 5U == g_charge_duty_permille);
    g_test_current = 3979U;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    g_test_current = 3980U;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state);
    assert(1U == g_charge_current_limited);
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());
    g_test_current = 4100U;
    duty = g_charge_duty_permille;
    writes = g_test_set_count;
    tick();
    assert(duty - 1U == g_charge_duty_permille);
    assert(writes + 1U == g_test_set_count);
    g_test_current = 3901U;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state);
    g_test_current = 3900U;
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(0U == g_charge_current_limited);
    assert(duty == g_charge_duty_permille);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    for(i = 0U; i < 49U; i++) { tick(); }
    assert(duty + 1U == g_charge_duty_permille);
    puts("PASS: independent MPPT/CC states, original 3980/3900 hysteresis, same-D fine resume");

    g_test_current = 2700U;
    g_test_battery = 14380U;
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());
    assert(duty == g_charge_duty_permille);
    assert(duty == g_charge_cv_duty_ceiling_permille);
    g_test_battery = 14301U;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_battery = 14300U;
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(duty == g_charge_duty_permille);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    assert(0U == g_charge_mppt_previous_power_valid);
    puts("PASS: original CV entry/exit/ceiling; release selects MPPT without a duty jump");

    g_test_battery = 14400U;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_battery = 14300U;
    g_test_current = 4000U;
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state);
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());
    assert(duty == g_charge_duty_permille);
    begin_tracking();
    g_test_current = 4000U;
    tick();
    g_test_battery = 14400U;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    puts("PASS: CV can release directly to CC; CV still preempts current regulation");

    /* Full confirmation enters maintenance directly, initially with PWM off. */
    begin_tracking();
    g_test_battery = 14410U;
    g_test_current = 510U; /* Net battery current exactly +500 mA. */
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_current = 511U; /* +501 mA must not count. */
    tick();
    assert(0U == g_charge_full_confirm_tick_count);
    g_test_current = 510U;
    for(i = 0U; i < 999U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(1U == g_charge_full_confirmed);
    assert(1000U == g_charge_full_confirm_tick_count);
    assert(0U == g_charge_duty_permille && 0U == g_test_pwm_on);
    writes = g_test_set_count;
    duty = (uint16_t)g_test_start_count;
    for(i = 0U; i < 20U; i++) { tick(); }
    assert(writes == g_test_set_count && duty == g_test_start_count);
    assert(14100U == svc_charge_config_get()->maintenance.voltage_target_mv);
    g_test_battery = 14101U;
    tick();
    assert(0U == g_test_pwm_on);
    g_test_battery = 14100U;
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(1U == g_test_pwm_on);
    assert(783U == g_test_initial_duty); /* Fresh 14.1/18 V duty, not old CV D. */
    g_test_battery = 14120U;
    tick();
    assert(1U == g_test_pwm_on); /* Exact deadband boundary is tolerated. */
    duty = g_charge_duty_permille;
    g_test_battery = 14121U;
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(duty - 1U == g_charge_duty_permille && 1U == g_test_pwm_on);
    {
        uint32_t starts = g_test_start_count;
        uint32_t stops = g_test_stop_count;
        g_test_current = 1400U;
        g_test_load = 1000U;
        for(i = 0U; i < 600U; i++)
        {
            g_test_battery = 14079U;
            tick();
            assert(duty == g_charge_duty_permille);
            g_test_battery = 14121U;
            tick();
            assert(duty - 1U == g_charge_duty_permille);
            assert(1U == g_test_pwm_on);
            assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
        }
        assert(starts == g_test_start_count && stops == g_test_stop_count);
    }
    enter_maintenance_idle();
    g_test_battery = 14100U;
    g_test_start_status = PLATFORM_ERR_HW;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(0U == g_test_pwm_on);
    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U;
    tick();
    assert(1U == g_test_pwm_on);
    g_test_stop_status = PLATFORM_ERR_HW;
    enter_maintenance_idle();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    g_test_stop_status = PLATFORM_ERR_OK;
    tick();
    assert(0U == g_test_pwm_on);
    puts("PASS: CV completion directly enters maintenance; 14.1 V restart,");
    puts("      continuous loaded CV across deadband, no stopped-PWM writes, start/stop errors");

    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U;
    tick();
    g_test_set_status = PLATFORM_ERR_HW;
    g_test_battery = 14121U;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    assert(0U == g_test_pwm_on);

    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U;
    tick();
    g_test_current = 0U;
    g_test_load = 500U;
    for(i = 0U; i < 5U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault);
    assert(0U == g_test_pwm_on);
    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U; /* Above old 13.8 V: no voltage prerequisite. */
    tick();
    g_test_current = 500U;
    g_test_load = 800U;
    g_charge_full_confirmed = 1U;
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    duty = g_charge_duty_permille;
    writes = g_test_stop_count;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(duty == g_charge_duty_permille && 1U == g_test_pwm_on);
    assert(writes == g_test_stop_count && 0U == g_charge_full_confirmed);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());

    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U;
    tick();
    g_test_current = 4000U;
    g_test_load = 5200U;
    for(i = 0U; i < 499U; i++) { tick(); }
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state);
    assert(duty == g_charge_duty_permille && 1U == g_test_pwm_on);
    assert(SVC_CHARGE_MPPT_PHASE_PAUSED == svc_charge_mppt_get_phase());

    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14100U;
    tick();
    g_test_current = 1000U;
    g_test_load = 1200U; /* Exact -200 mA counts; -199 mA resets. */
    for(i = 0U; i < 499U; i++) { tick(); }
    g_test_load = 1199U;
    tick();
    assert(0U == g_charge_maintenance_recharge_tick_count);
    g_test_load = 1200U;
    g_test_battery = 14380U; /* CV must still preempt on normal handover. */
    for(i = 0U; i < 499U; i++) { tick(); }
    duty = g_charge_duty_permille;
    tick();
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(duty == g_charge_duty_permille && 1U == g_test_pwm_on);

    begin_tracking();
    enter_maintenance_idle();
    g_test_battery = 14200U; /* Waiting with PWM off also observes discharge. */
    g_test_load = 200U;
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    assert(0U == g_charge_duty_permille && 0U == g_test_pwm_on);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(1U == g_test_pwm_on);
    puts("PASS: maintenance net-discharge handover without voltage gate/fault,");
    puts("      5 s boundary/reset, same-D MPPT/CC/CV choice, idle checked startup");
    puts("PASS: maintenance 50 ms reverse-power guard still shuts PWM off");

    {
        svc_measure_value_t completion_value = {0};
        completion_value.battery_voltage_mv = 14400U;
        svc_charge_maintenance_reset();
        completion_value.battery_current_ma = -500;
        for(i = 0U; i < 999U; i++)
        {
            assert(0U == svc_charge_maintenance_update_completion(&completion_value));
        }
        completion_value.battery_current_ma = -501;
        assert(0U == svc_charge_maintenance_update_completion(&completion_value));
        assert(0U == g_charge_full_confirm_tick_count);
        completion_value.battery_current_ma = -500;
        for(i = 0U; i < 999U; i++)
        {
            assert(0U == svc_charge_maintenance_update_completion(&completion_value));
        }
        assert(1U == svc_charge_maintenance_update_completion(&completion_value));
        assert(1000U == g_charge_full_confirm_tick_count);
    }
    puts("PASS: full-current window includes +/-500 mA, rejects +/-501 mA");

    begin_tracking();
    g_test_measure_status = PLATFORM_ERR_HW;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(0U == g_test_pwm_on);
    assert(SVC_CHARGE_MPPT_PHASE_OFF == svc_charge_mppt_get_phase());
    begin_tracking();
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    assert(SVC_CHARGE_MPPT_PHASE_OFF == svc_charge_mppt_get_phase());
    assert(0U == g_test_pwm_on);
    puts("PASS: faults and stop reset MPPT phase and shut PWM off");
    return 0;
}
#ifdef TRACE_RUN
static unsigned int normalize_state(svc_charge_state_t state)
{
    return (SVC_CHARGE_STATE_MPPT == state) ? 4U : (unsigned int)state;
}
int main(void)
{
    unsigned int i;
    begin_tracking();
    for(i = 0U; i < 12000U; i++)
    {
        unsigned int slot = i % 1600U;
        g_test_current = slot < 300U ? 2700U : slot < 600U ? 4050U :
            slot < 900U ? 3950U : 2700U;
        g_test_battery = slot >= 1000U && slot < 1300U ? 14400U : 13700U;
        g_test_on_vin = i % 2700U >= 2650U ? 13000U : 18000U;
        if(1700U == i || 7000U == i) { assert(PLATFORM_ERR_OK == svc_charge_stop()); }
        if(1750U == i || 7050U == i) { assert(PLATFORM_ERR_OK == svc_charge_start()); }
        tick();
        printf("%u,%u,%u,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", i,
            normalize_state(g_charge_state), g_charge_fault, g_charge_status,
            g_charge_duty_permille, g_charge_current_error_ma, g_charge_voltage_error_mv,
            g_charge_duty_high_limited, g_charge_cv_duty_ceiling_permille,
            g_charge_cv_duty_ceiling_active, g_charge_cv_duty_ceiling_limited,
            g_test_pwm_on, g_test_start_count, g_test_stop_count, g_test_set_count,
            g_charge_mppt_power_raw, g_charge_mppt_previous_power_valid,
            g_charge_mppt_direction_increase, g_charge_mppt_compare_count,
            g_charge_full_confirm_tick_count);
    }
    return 0;
}
#else
int main(void) { return run_split_tests(); }
#endif
'@
Push-Location $repoRoot
try {
    $common = @('02_Service/svc_charge_config.c', '02_Service/svc_charge_control.c',
        '02_Service/svc_charge_cc.c', '02_Service/svc_charge_cv.c',
        '02_Service/svc_charge_maintenance.c', '02_Service/svc_charge_protection.c')
    $flags = @('-x','c','-std=c99','-Wall','-Wextra','-Werror',
        '-I02_Service','-I03_Platform/bsp','-I03_Platform/comm','-I03_Platform/mcu_interface')
    $executable = Join-Path $testDirectory 'split.exe'
    $source | & $Gcc @flags -o $executable - @common
    if ($LASTEXITCODE -ne 0) { throw 'State split host compile failed' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'State split assertions failed' }
    if ($BaselineDirectory) {
        $baselineRoot = (Resolve-Path -LiteralPath $BaselineDirectory).Path
        $baselineSource = $source.Replace('"svc_charge.c"',
            '"' + (Join-Path $baselineRoot 'svc_charge.c').Replace('\','/') + '"')
        $baselineCommon = @('02_Service/svc_charge_config.c',
            (Join-Path $baselineRoot 'svc_charge_control.c'),
            (Join-Path $baselineRoot 'svc_charge_maintenance.c'),
            '02_Service/svc_charge_protection.c')
        $traces = @()
        foreach ($version in @('baseline','current')) {
            $traceSource = if ($version -eq 'baseline') { $baselineSource } else { $source }
            $traceCommon = if ($version -eq 'baseline') { $baselineCommon } else { $common }
            $traceExe = Join-Path $testDirectory ($version + '.exe')
            $traceSource | & $Gcc @flags -DTRACE_RUN -include 02_Service/svc_charge_cc.h `
                -include 02_Service/svc_charge_cv.h -o $traceExe - @traceCommon
            if ($LASTEXITCODE -ne 0) { throw "Trace compile failed: $version" }
            $trace = & $traceExe
            if ($LASTEXITCODE -ne 0) { throw "Trace run failed: $version" }
            $traces += ,$trace
        }
        for ($i = 0; $i -lt $traces[0].Count; $i++) {
            if ($traces[0][$i] -cne $traces[1][$i]) {
                throw "Trace differs at tick ${i}:`nbefore $($traces[0][$i])`nafter  $($traces[1][$i])"
            }
        }
        Write-Output 'PASS: 12000 pre/post split traces match (MPPT state normalized to old 4)'
    }
    Write-Output "Host executables retained at $testDirectory"
} finally { Pop-Location }
