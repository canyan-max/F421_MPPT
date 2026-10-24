param(
    [string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe'
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
void bsp_measure_fast_ovp_enable(uint8_t enabled) { (void)enabled; }

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
extern svc_charge_maintenance_phase_t g_charge_maintenance_phase;
extern uint32_t g_charge_maintenance_build_timeout_count;
extern uint32_t g_charge_maintenance_low_output_wait_count;
extern uint8_t g_charge_cv_load_active;
extern uint32_t g_charge_cv_no_load_ticks;
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
#if SVC_CHARGE_ENABLE_MAINTENANCE
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

    /* Full confirmation alone must not interrupt a connected 1 A load. */
    begin_tracking();
    g_test_battery = 14403U;
    g_test_current = 1205U;
    g_test_load = 1047U;
    tick();
    duty = g_charge_duty_permille;
    writes = g_test_stop_count;
    for(i = 0U; i < 1250U; i++) { tick(); }
    assert(1U == g_charge_full_confirmed);
    assert(1000U == g_charge_full_confirm_tick_count);
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(1U == g_test_pwm_on && duty == g_charge_duty_permille);
    assert(writes == g_test_stop_count);
    assert(1U == g_charge_cv_load_active && 0U == g_charge_cv_no_load_ticks);
    g_test_load = (SVC_CHARGE_LOAD_ON_MA + SVC_CHARGE_LOAD_OFF_MA) / 2U;
    for(i = 0U; i < 200U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(1U == g_charge_cv_load_active); /* Inside hysteresis: retain presence. */
    g_test_load = 20U;
    for(i = 0U; i < 99U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_load = 100U;
    tick(); /* Exact ON threshold cancels the pending no-load decision. */
    assert(0U == g_charge_cv_no_load_ticks);
    g_test_load = 20U;
    for(i = 0U; i < 99U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(0U == g_test_pwm_on && 0U == g_charge_duty_permille);
    assert(1U == g_charge_full_confirmed);
    puts("PASS: full CV keeps 1 A load powered; configured load hysteresis and");
    puts("      continuous 1 s no-load decision before maintenance shutdown");

    /* No-load qualification follows full confirmation, then PWM stops. */
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
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(1U == g_charge_full_confirmed);
    assert(1000U == g_charge_full_confirm_tick_count);
    for(i = 0U; i < 98U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
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
    enter_charge_wait();
    g_test_battery = 14100U;
    g_test_start_status = PLATFORM_ERR_HW;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(0U == g_test_pwm_on);
    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14100U;
    tick();
    assert(1U == g_test_pwm_on);
    g_test_stop_status = PLATFORM_ERR_HW;
    enter_charge_wait();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    g_test_stop_status = PLATFORM_ERR_OK;
    tick();
    assert(0U == g_test_pwm_on);
    puts("PASS: full/no-load confirmation enters retained maintenance; 14.1 V restart,");
    puts("      continuous loaded CV across deadband, no stopped-PWM writes, start/stop errors");

    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14100U;
    tick();
    g_test_set_status = PLATFORM_ERR_HW;
    g_test_battery = 14121U;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    assert(0U == g_test_pwm_on);

    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14100U;
    tick();
    g_test_current = 100U;
    tick(); tick(); /* Establish supply before checking the running guard. */
    g_test_battery = 14070U; /* Actual supply demand, below voltage deadband. */
    g_test_current = 0U;
    g_test_load = 500U;
    for(i = 0U; i < 5U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault);
    assert(0U == g_test_pwm_on);
    begin_tracking();
    enter_charge_wait();
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
    enter_charge_wait();
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
    enter_charge_wait();
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
    enter_charge_wait();
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
    puts("PASS: maintenance low-output fault retained when more supply is required");

    /* Reproduce CV reaching maintenance, then intentional CV reduction at
     * 14.130 V with a 1 A load. Low output must NOT become input fault 3. */
    begin_tracking();
    enter_charge_wait();
    g_charge_full_confirmed = 1U;
    g_test_battery = 14100U;
    g_test_load = 1000U;
    tick();
    g_test_current = 1100U;
    tick(); tick();
    assert(SVC_CHARGE_MAINTENANCE_PHASE_RUNNING == g_charge_maintenance_phase);
    g_test_battery = 14130U;
    g_test_current = 37U;
    writes = g_charge_maintenance_low_output_wait_count;
    for(i = 0U; i < 4U; i++) { tick(); }
    assert(1U == g_test_pwm_on);
    tick();
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(SVC_CHARGE_MAINTENANCE_PHASE_WAIT == g_charge_maintenance_phase);
    assert(writes + 1U == g_charge_maintenance_low_output_wait_count);
    assert(0U == g_test_pwm_on && 0U == g_charge_duty_permille);
    assert(1U == g_charge_full_confirmed && 0U == g_charge_fault_snapshot_valid);
    g_test_battery = 14100U;
    tick();
    assert(0U == g_test_pwm_on); /* Do not immediately restart at 14.1 V. */
    g_test_battery = 14081U;
    tick();
    assert(0U == g_test_pwm_on);
    g_test_battery = 14080U;
    tick();
    assert(1U == g_test_pwm_on);
    assert(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase);
    g_test_current = 1100U;
    tick(); tick();
    assert(SVC_CHARGE_MAINTENANCE_PHASE_RUNNING == g_charge_maintenance_phase);
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    g_test_current = 37U;
    g_test_battery = 14130U;
    for(i = 0U; i < 4U; i++) { tick(); }
    g_test_stop_status = PLATFORM_ERR_HW;
    tick(); /* A failed normal OFF must still become a power-path fault. */
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    g_test_stop_status = PLATFORM_ERR_OK;
    tick();
    assert(0U == g_test_pwm_on);
    puts("PASS: intentional low output enters normal WAIT without fault/CV restart,");
    puts("      full flag preserved, deadband restart boundary, OFF failure retained");

    /* Regression: 20 V input, 1 A load, restart exactly inside CV deadband. */
    begin_tracking();
    enter_charge_wait();
    g_test_off_vin = 20000U;
    g_test_on_vin = 20000U;
    g_test_battery = 14099U;
    g_test_current = 0U;
    g_test_load = 1000U;
    tick();
    assert(705U == g_charge_duty_permille);
    assert(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase);
    for(i = 0U; i < 6U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(708U == g_charge_duty_permille); /* One step per 20 ms, not held. */
    g_test_current = 100U;
    tick();
    assert(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase);
    g_test_current = 99U;
    tick(); /* Confirmation must be continuous. */
    g_test_current = 100U;
    tick();
    assert(SVC_CHARGE_MAINTENANCE_PHASE_BUILD == g_charge_maintenance_phase);
    tick();
    assert(SVC_CHARGE_MAINTENANCE_PHASE_RUNNING == g_charge_maintenance_phase);
    duty = g_charge_duty_permille;
    g_test_current = 1100U;
    for(i = 0U; i < 10U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    assert(duty == g_charge_duty_permille && 1U == g_test_pwm_on);

    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14100U;
    g_test_current = 0U;
    g_test_load = 1000U;
    tick();
    writes = g_charge_maintenance_build_timeout_count;
    for(i = 0U; i < 29U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MAINTENANCE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault);
    assert(writes + 1U == g_charge_maintenance_build_timeout_count);
    assert(0U == g_test_pwm_on);
    assert(SVC_CHARGE_MAINTENANCE_PHASE_IDLE == g_charge_maintenance_phase);

    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14100U;
    g_test_current = 0U;
    tick();
    duty = g_charge_duty_permille;
    g_test_battery = 14121U;
    tick(); /* Voltage down-step must not be delayed by the build interval. */
    assert(duty - 1U == g_charge_duty_permille);
    g_test_battery = 14100U;
    g_test_current = 4100U;
    duty = g_charge_duty_permille;
    tick();
    assert(duty - 1U == g_charge_duty_permille); /* Current limit wins too. */
    g_test_current = 12000U;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == g_charge_fault);
    assert(0U == g_test_pwm_on);
    puts("PASS: maintenance deadband startup builds supply at 1/20 ms,");
    puts("      continuous 100 mA confirmation, bounded timeout, voltage/current priority");

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
#else
int run_standby_tests(void)
{
    unsigned int i;
    uint16_t duty;
    uint32_t starts;
    uint32_t stops;
    begin_tracking();
    g_test_battery = 14403U;
    g_test_current = 1205U;
    g_test_load = 1047U;
    tick();
    duty = g_charge_duty_permille;
    stops = g_test_stop_count;
    for(i = 0U; i < 1250U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(1U == g_charge_full_confirmed);
    assert(1U == g_test_pwm_on && duty == g_charge_duty_permille);
    assert(stops == g_test_stop_count);
    g_test_load = 20U;
    g_test_current = 200U;
    for(i = 0U; i < 99U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    assert(0U == g_test_pwm_on && 0U == g_charge_duty_permille);
    assert(1U == g_charge_full_confirmed);
    assert(SVC_CHARGE_MAINTENANCE_PHASE_IDLE == g_charge_maintenance_phase);
    starts = g_test_start_count;
    g_test_battery = 14090U; /* Below old 14.1 V: must NOT start maintenance. */
    for(i = 0U; i < 500U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    assert(starts == g_test_start_count && 0U == g_test_pwm_on);
    g_test_off_vin = 0U;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    g_test_load = 100U;
    tick();
    g_test_load = 20U;
    tick();
    assert(0U == g_charge_full_wait_load_ticks);
    g_test_load = 100U;
    tick(); tick();
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state); /* Input absent. */
    g_test_off_vin = 18000U;
    tick();
    assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    assert(0U == g_charge_full_confirmed && 0U == g_test_pwm_on);
    tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(783U == g_test_initial_duty && 1U == g_test_pwm_on);
    tick(); tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    puts("PASS: loaded full CV stays powered; no-load full standby never runs maintenance,");
    puts("      absent input waits, load confirmation wakes checked normal startup");

    begin_tracking();
    enter_charge_wait();
    g_charge_full_confirmed = 1U;
    g_test_battery = 13800U;
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    g_test_battery = 13801U;
    tick();
    assert(0U == g_charge_full_wait_recharge_ticks);
    g_test_battery = 13800U;
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(0U == g_charge_full_confirmed);
    puts("PASS: no-load recharge uses 13.8 V/5 s boundary and continuous confirmation");

    begin_tracking();
    enter_charge_wait();
    g_test_measure_status = PLATFORM_ERR_HW;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_MEASUREMENT == g_charge_fault);
    begin_tracking();
    enter_charge_wait();
    g_test_off_vin = 0U;
    g_test_temperature = SVC_MEASURE_TEMPERATURE_INVALID;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_TEMPERATURE_SENSOR == g_charge_fault);
    begin_tracking();
    enter_charge_wait();
    g_test_battery = 14600U;
    tick();
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == g_charge_fault);
    begin_tracking();
    enter_charge_wait();
    svc_charge_process();
    svc_charge_process(); /* No new sequence: sampling timeout still applies. */
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_SAMPLE_TIMEOUT == g_charge_fault);
    begin_tracking();
    enter_charge_wait();
    g_test_off_vin = 0U;
    g_test_battery = 14400U;
    g_test_temperature = 650;
    for(i = 0U; i < 199U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FULL_WAIT == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_TEMPERATURE_HIGH == g_charge_fault);
    begin_tracking();
    enter_charge_wait();
    g_test_current = 12000U;
    /* Stub models measured current only while PWM is on; force a sensor
     * residual to check the shared standby current guard directly. */
    {
        svc_measure_value_t value;
        svc_charge_fault_t fault;
        uint8_t fresh;
        g_test_pwm_on = 1U;
        g_test_on_vin = 0U;
        g_test_sequence++;
        assert(PLATFORM_ERR_HW == get_new_measurement(
            &value, &fresh, &fault, SVC_CHARGE_SAMPLE_STANDBY));
        assert(SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == fault);
        g_test_pwm_on = 0U;
    }
    begin_tracking();
    g_test_stop_status = PLATFORM_ERR_HW;
    enter_charge_wait();
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault);
    g_test_stop_status = PLATFORM_ERR_OK;
    tick();
    assert(0U == g_test_pwm_on);
    begin_tracking();
    enter_charge_wait();
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    g_test_load = 1000U;
    g_test_battery = 13700U;
    for(i = 0U; i < 600U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_OFF == g_charge_state && 0U == g_test_pwm_on);
    puts("PASS: standby sensor/overvoltage/current/temperature/stale guards,");
    puts("      OFF failure and manual stop remain protected");
    return 0;
}
#endif

static void run_overvoltage_tests(void)
{
    unsigned int i;
    svc_measure_value_t value = {0};
    begin_tracking();
    assert(14400U == svc_charge_config_get()->voltage_target_mv);
    assert(14600U == svc_charge_config_get()->battery_overvoltage_mv);
    value.input_voltage_mv = 20000U;
    value.battery_voltage_mv = 14599U;
    value.temperature_deci_c = 250;
    assert(SVC_CHARGE_FAULT_NONE == svc_charge_protection_check_immediate(&value));
    assert(SVC_CHARGE_FAULT_NONE == svc_charge_protection_check_wake(&value));
    assert(SVC_CHARGE_FAULT_NONE == svc_charge_protection_check_start_voltage(&value));
    value.battery_voltage_mv = 14600U;
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == svc_charge_protection_check_immediate(&value));
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == svc_charge_protection_check_wake(&value));
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == svc_charge_protection_check_start_voltage(&value));
    for(i = 0U; i < 4U; i++)
    {
        begin_tracking();
        if(1U == i) { g_test_current = 4000U; tick(); }
        if(2U == i) { g_test_battery = 14400U; tick(); }
        if(3U == i)
        {
            enter_charge_wait();
        }
        g_test_battery = 14600U;
        tick();
        assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
        assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == g_charge_fault);
        assert(0U == g_test_pwm_on && 0U == g_charge_duty_permille);
    }
    value.battery_voltage_mv = 14401U;
    assert(0U == svc_charge_protection_is_recovery_condition_met(
        SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE, &value));
    value.battery_voltage_mv = 14400U;
    assert(1U == svc_charge_protection_is_recovery_condition_met(
        SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE, &value));
    g_test_battery = 14400U;
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    assert(PLATFORM_ERR_OK == svc_charge_config_init(SVC_CHARGE_BATTERY_SYSTEM_24V));
    assert(svc_charge_config_get()->voltage_target_mv <
        svc_charge_config_get()->battery_overvoltage_mv);
    value.input_voltage_mv = 40000U;
    value.battery_voltage_mv = svc_charge_config_get()->battery_overvoltage_mv - 1U;
    assert(SVC_CHARGE_FAULT_NONE == svc_charge_protection_check_immediate(&value));
    value.battery_voltage_mv = svc_charge_config_get()->battery_overvoltage_mv;
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == svc_charge_protection_check_immediate(&value));
    begin_tracking();
    puts("PASS: independent 14.6 V OVP boundary across runtime/standby, PWM OFF,");
    puts("      unchanged 14.4 V CV/recovery +5 s; separate 24 V threshold");
}

int main(void)
{
    run_overvoltage_tests();
#if SVC_CHARGE_ENABLE_MAINTENANCE
    return run_split_tests();
#else
    return run_standby_tests();
#endif
}
'@
Push-Location $repoRoot
try {
    $common = @('02_Service/svc_charge_config.c', '02_Service/svc_charge_control.c',
        '02_Service/svc_charge_cc.c', '02_Service/svc_charge_cv.c',
        '02_Service/svc_charge_maintenance.c', '02_Service/svc_charge_protection.c')
    $flags = @('-x','c','-std=c99','-Wall','-Wextra','-Werror',
        '-I02_Service','-I03_Platform/bsp','-I03_Platform/comm','-I03_Platform/mcu_interface')
    foreach ($maintenanceMode in @(0, 1)) {
        $executable = Join-Path $testDirectory "split-$maintenanceMode.exe"
        $source | & $Gcc @flags "-DSVC_CHARGE_ENABLE_MAINTENANCE=$maintenanceMode" -o $executable - @common
        if ($LASTEXITCODE -ne 0) { throw "State host compile failed, maintenance=$maintenanceMode" }
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "State assertions failed, maintenance=$maintenanceMode" }
    }
    Write-Output "Host executables retained at $testDirectory"
} finally { Pop-Location }
