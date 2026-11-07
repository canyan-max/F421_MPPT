param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('duty-reference-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include "svc_charge.c"
#include "svc_charge_mppt.c"
static uint32_t g_test_vin = 20000U;
static uint32_t g_test_bat = 13500U;
static uint32_t g_test_current = 700U;
static uint32_t g_test_load;
static platform_err_t g_test_stop_status = PLATFORM_ERR_OK;
static platform_err_t g_test_set_status = PLATFORM_ERR_OK;
static uint32_t g_test_sequence;
static uint32_t g_test_writes;
static uint8_t g_test_pwm;
static uint16_t g_test_duty;
platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    *p_value = (svc_measure_value_t){0};
    p_value->input_voltage_mv = g_test_vin;
    p_value->battery_voltage_mv = g_test_bat;
    p_value->input_current_ma = g_test_pwm ? g_test_current : 0U;
    p_value->output_current_ma = g_test_load;
    p_value->battery_current_ma = (int32_t)p_value->input_current_ma - (int32_t)g_test_load;
    p_value->temperature_deci_c = 250;
    return PLATFORM_ERR_OK;
}
uint32_t svc_measure_get_sequence(void) { return g_test_sequence; }
uint8_t svc_measure_is_ready(void) { return 1U; }
void bsp_measure_fast_ovp_enable(uint8_t enabled) { (void)enabled; }
platform_err_t bsp_power_path_charge_start(uint16_t duty)
{
    g_test_pwm = 1U; g_test_duty = duty; g_test_writes++;
    return PLATFORM_ERR_OK;
}
platform_err_t bsp_power_path_charge_set_duty(uint16_t duty)
{
    uint64_t floor;
    assert(g_test_pwm);
    if(PLATFORM_ERR_OK != g_test_set_status) { return g_test_set_status; }
    assert(0U != g_charge_input_voc_mv);
    floor = ((uint64_t)g_test_bat * 1000U + g_charge_input_voc_mv / 2U) /
        g_charge_input_voc_mv;
    assert(duty >= floor && duty <= 950U);
    assert(duty <= g_charge_input_duty_max_permille);
    g_test_duty = duty; g_test_writes++;
    return PLATFORM_ERR_OK;
}
platform_err_t bsp_power_path_charge_stop(void)
{
    if(PLATFORM_ERR_OK != g_test_stop_status) { return g_test_stop_status; }
    g_test_pwm = 0U;
    return PLATFORM_ERR_OK;
}
static void tick(void) { g_test_sequence++; svc_charge_process(); }
static void begin_at(uint32_t vin, uint32_t bat)
{
    g_test_vin = vin; g_test_bat = bat; g_test_current = 700U;
    g_test_load = 0U; g_test_stop_status = PLATFORM_ERR_OK;
    g_test_set_status = PLATFORM_ERR_OK;
    assert(PLATFORM_ERR_OK == svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_12V));
    assert(PLATFORM_ERR_OK == svc_charge_start());
    tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(vin == g_charge_input_voc_mv);
    tick(); tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
}
static void begin(void) { begin_at(20000U, 13500U); }
static void test_ratio_helpers(void)
{
    svc_measure_value_t value = {0};
    uint16_t reference;
    uint16_t expected;
    uint16_t duty;
    uint32_t voc;
    uint32_t bat;
    for(voc = 14001U; voc <= 48000U; voc += 997U)
    {
        for(bat = 10000U; bat < voc && bat <= 14500U; bat += 103U)
        {
            /* Actual loaded Vin deliberately differs from the saved Voc. */
            value.input_voltage_mv = bat + 1000U;
            value.battery_voltage_mv = bat;
            assert(PLATFORM_ERR_OK ==
                svc_charge_control_calculate_duty_reference(&value, voc, &expected));
            assert(expected == ((uint64_t)bat * 1000U + voc / 2U) / voc);
            for(duty = 1U; duty <= 950U; duty++)
            {
                reference = 675U;
                assert((duty >= expected) == (PLATFORM_ERR_OK ==
                    svc_charge_control_check_duty_reference(&value, voc, duty, &reference)));
                assert(expected == reference);
            }
        }
    }
    value.input_voltage_mv = 18000U; value.battery_voltage_mv = 14000U;
    assert(PLATFORM_ERR_OK == svc_charge_control_calculate_duty_reference(&value, 20000U, &reference));
    assert(700U == reference);
    assert(PLATFORM_ERR_OK == svc_charge_control_check_duty_reference(&value, 20000U, 700U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 20000U, 699U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 0U, 700U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 14000U, 700U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 20000U, 0U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 20000U, 951U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_duty_reference(NULL, 20000U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_duty_reference(&value, 20000U, NULL));
    value.battery_voltage_mv = 0U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_duty_reference(&value, 20000U, &reference));
    value.battery_voltage_mv = 14000U; value.input_voltage_mv = 14000U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 20000U, 700U, &reference));
    value.input_voltage_mv = 48000U; value.battery_voltage_mv = 5000U;
    assert(PLATFORM_ERR_OK == svc_charge_control_check_duty_reference(&value, 48000U, 104U, &reference));
    assert(104U == reference);
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 48000U, 103U, &reference));
    value.input_voltage_mv = 20000U; value.battery_voltage_mv = 13500U;
    assert(PLATFORM_ERR_OK == svc_charge_control_calculate_initial_duty(&value, &duty));
    assert(685U == duty);
    value.battery_voltage_mv = 18900U;
    assert(PLATFORM_ERR_OK == svc_charge_control_calculate_initial_duty(&value, &duty));
    assert(950U == duty);
    value.battery_voltage_mv = 19020U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_initial_duty(&value, &duty));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_initial_duty(NULL, &duty));
    puts("PASS: explicit Voc ratio, changing battery, stale cache, invalid reference, startup offset");
}
static void test_input_and_clamp(void)
{
    const svc_charge_state_t states[] = {
        SVC_CHARGE_STATE_CONSTANT_CURRENT,
        SVC_CHARGE_STATE_CONSTANT_VOLTAGE,
        SVC_CHARGE_STATE_MPPT
    };
    unsigned int i;
    uint32_t writes;
    uint16_t duty;
    svc_measure_value_t value = {0};
    svc_charge_fault_t fault;
    for(i = 0U; i < sizeof(states) / sizeof(states[0U]); i++)
    {
        begin();
        assert(675U == g_charge_duty_min_reference_permille && 685U == g_test_duty);
        g_charge_state = states[i];
        g_test_bat = 14000U; g_test_vin = 17000U;
        assert(PLATFORM_ERR_OK == svc_measure_get(&value));
        duty = 690U;
        assert(PLATFORM_ERR_OK == apply_charge_duty(&value, &duty, 0U, &fault));
        g_charge_duty_permille = duty;
        assert(700U == duty && 700U == g_test_duty);
        assert(700U == g_charge_duty_min_reference_permille);
        assert(950U == g_charge_input_duty_max_permille);
        assert(0U == g_charge_input_range_conflict && 0U == g_charge_input_limited);
        g_test_vin = 22000U;
        assert(PLATFORM_ERR_OK == svc_measure_get(&value));
        duty = 699U;
        assert(PLATFORM_ERR_OK == apply_charge_duty(&value, &duty, 0U, &fault));
        assert(700U == duty && 700U == g_charge_duty_min_reference_permille);
        assert(0U == g_charge_input_limited);
        g_test_bat = 14400U;
        assert(PLATFORM_ERR_OK == svc_measure_get(&value));
        duty = 700U;
        assert(PLATFORM_ERR_OK == apply_charge_duty(&value, &duty, 0U, &fault));
        assert(720U == duty && 720U == g_charge_duty_min_reference_permille);
        duty = 900U;
        assert(PLATFORM_ERR_OK == apply_charge_duty(&value, &duty, 0U, &fault));
        assert(900U == duty && 950U == g_charge_input_duty_max_permille);
        assert(20000U == g_charge_input_voc_mv && 17500U == g_charge_input_reference_mv);
    }
    /* Reproduce the former min755 > max752 fault frame. */
    begin_at(20072U, 13500U);
    g_test_vin = 17512U; g_test_bat = 13216U;
    g_test_duty = g_charge_duty_permille = 752U;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 1U == g_test_pwm);
    assert(17563U == g_charge_input_reference_mv);
    assert(658U == g_charge_duty_min_reference_permille);
    assert(950U == g_charge_input_duty_max_permille);
    assert(0U == g_charge_input_range_conflict && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* START_SETTLE recalculates its candidate from live Vin, but the floor uses Voc. */
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    g_test_vin = 20000U; g_test_bat = 13500U;
    assert(PLATFORM_ERR_OK == svc_charge_start()); tick();
    assert(685U == g_test_duty);
    g_test_vin = 17000U; tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(675U == g_charge_duty_min_reference_permille);
    assert(804U == g_test_duty); /* No Voc-derived upper clamp. */
    /* An actually impossible saved-Voc floor still stops before any PWM write. */
    begin_at(15000U, 13500U);
    g_test_bat = 14400U; writes = g_test_writes;
    tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    assert(960U == g_charge_duty_min_reference_permille);
    assert(1U == g_charge_input_range_conflict && writes == g_test_writes);
    puts("PASS: common CC/CV/MPPT floor, fixed950 ceiling, Vin drop/rise, former conflict, settle");
}
static void test_cv_and_mppt(void)
{
    unsigned int i;
    uint32_t writes;
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14450U; g_test_vin = 18000U;
    g_test_duty = g_charge_duty_permille = 723U;
    writes = g_test_writes;
    for(i = 0U; i < 100U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(723U == g_test_duty && writes == g_test_writes);
    assert(723U == g_charge_cv_observation.duty_reference_permille);
    assert(723U == g_charge_cv_observation.duty_min_permille);
    assert(1U == g_charge_cv_observation.downward_blocked);
    g_test_vin = 17000U; tick();
    assert(723U == g_test_duty && 723U == g_charge_duty_min_reference_permille);
    assert(723U == g_charge_cv_observation.duty_min_permille);
    assert(17000U == g_charge_cv_observation.input_voltage_mv);
    g_test_current = 100U; tick();
    assert(1U == g_charge_cv_observation.low_current);
    g_test_current = 101U; tick();
    assert(0U == g_charge_cv_observation.low_current);
    g_test_current = 1500U; g_test_load = 1500U; tick();
    assert(1500U == g_charge_cv_observation.total_output_current_ma);
    assert(0U == g_charge_cv_observation.low_current);
    g_test_bat = 14370U; tick();
    assert(722U == g_test_duty && SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_bat = 14300U; tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 1U == g_test_pwm);
    assert(0U == g_charge_input_limited);
    assert(722U == g_test_duty);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    begin();
    g_test_bat = 13380U; g_test_vin = 18000U;
    g_test_duty = g_charge_duty_permille = 669U;
    svc_charge_mppt_reset(); g_charge_mppt_direction_increase = 0U;
    writes = g_test_writes;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(669U == g_charge_duty_min_reference_permille && 669U == g_test_duty);
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && writes == g_test_writes);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(670U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    assert(0U == g_charge_cv_observation.valid && 0U == g_charge_input_voc_mv);
    puts("PASS: CV diagnostics match saved-Voc floor, current observation, CV exit, MPPT lower probe");
}
static void test_cv_input_constraint(void)
{
    unsigned int i;
    uint32_t writes;
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14370U; g_test_vin = 20000U;
    g_test_duty = g_charge_duty_permille = 930U;
    writes = g_test_writes;
    tick();
    assert(931U == g_test_duty && writes + 1U == g_test_writes);
    assert(0U == g_charge_input_limited && 950U == g_charge_input_duty_max_permille);
    g_test_vin = 17499U;
    writes = g_test_writes; tick();
    assert(930U == g_test_duty && writes + 1U == g_test_writes);
    assert(1U == g_charge_input_limited && 0U == g_charge_duty_high_limited);
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    svc_charge_process(); /* A repeated sample cannot perform another step. */
    assert(930U == g_test_duty && writes + 1U == g_test_writes);
    tick();
    assert(929U == g_test_duty && writes + 2U == g_test_writes);
    /* Recover into the hysteresis band: hold instead of continuing to shed. */
    g_test_vin = 17500U; writes = g_test_writes; tick();
    assert(929U == g_test_duty && writes == g_test_writes && 1U == g_charge_input_limited);
    g_test_vin = 17699U; tick();
    assert(929U == g_test_duty && writes == g_test_writes && 1U == g_charge_input_limited);
    g_test_vin = 17700U; tick();
    assert(930U == g_test_duty && writes + 1U == g_test_writes && 0U == g_charge_input_limited);
    /* Once released, Vin in the band does not re-enter limiting. */
    g_test_vin = 17501U; tick();
    assert(931U == g_test_duty && 0U == g_charge_input_limited);
    /* CV/CC reductions win in both the low-input and recovery bands. */
    g_test_vin = 17499U; g_test_bat = 14450U;
    writes = g_test_writes; tick();
    assert(930U == g_test_duty && writes + 1U == g_test_writes);
    assert(1U == g_charge_input_limited); /* Exactly one decrement, not two. */
    g_test_vin = 17550U; tick();
    assert(929U == g_test_duty && 1U == g_charge_input_limited);
    g_test_bat = 14370U; g_test_current = 10100U; tick();
    assert(928U == g_test_duty && 1U == g_charge_input_limited);
    /* Dmin blocks further shedding without introducing a fault/PWM stop.
     * Net battery current is negative; frontend still contributes 500 mA. */
    g_test_vin = 17000U; g_test_current = 500U; g_test_load = 1000U;
    for(i = 0U; i < 300U; i++) { tick(); }
    assert(719U == g_test_duty && 719U == g_charge_duty_min_reference_permille);
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault && 1U == g_charge_input_limited);
    assert(1U == g_charge_cv_observation.downward_blocked);
    writes = g_test_writes;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(719U == g_test_duty && writes == g_test_writes);
    /* A prior attempt to increase at 950 must not leave input-loss armed. */
    g_test_duty = g_charge_duty_permille = 950U;
    g_test_current = 0U; g_test_load = 1000U; tick();
    assert(949U == g_test_duty && 0U == g_charge_duty_high_limited);
    /* Existing CV exit keeps D and hands over to fresh MPPT acquisition. */
    g_test_current = 500U; g_test_bat = 14300U; tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 949U == g_test_duty);
    assert(0U == g_charge_input_limited && SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    /* Below-reference input is not a new constraint on MPPT/CC. */
    begin(); g_test_vin = 17000U;
    g_test_duty = g_charge_duty_permille = 900U;
    svc_charge_mppt_reset();
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 901U == g_test_duty);
    assert(0U == g_charge_input_limited);
    begin(); g_test_vin = 17000U; g_test_current = 9950U;
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
    g_test_duty = g_charge_duty_permille = 900U; tick();
    assert(SVC_CHARGE_STATE_CONSTANT_CURRENT == g_charge_state && 901U == g_test_duty);
    assert(0U == g_charge_input_limited);
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    assert(0U == g_charge_input_limited);
    puts("PASS: CV input step once/frame, hysteresis hold/release, CC/CV priority, floor hold,");
    puts("      partial load supply, no false high-limit, unchanged MPPT/CC, smooth CV exit");
}
static void test_faults_and_recovery(void)
{
    unsigned int i;
    uint32_t writes;
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14600U; writes = g_test_writes; tick();
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == g_charge_fault);
    assert(0U == g_test_pwm && writes == g_test_writes);
    begin(); g_test_current = 12000U; tick();
    assert(SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == g_charge_fault && 0U == g_test_pwm);
    begin(); g_test_vin = g_test_bat; tick();
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault && 0U == g_test_pwm);
    begin(); g_charge_input_voc_mv = 0U; writes = g_test_writes; tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    assert(writes == g_test_writes);
    begin(); g_charge_input_reference_mv = 0U; writes = g_test_writes; tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    assert(writes == g_test_writes);
    begin(); g_test_bat = 0U; tick();
    assert(0U == g_test_pwm && SVC_CHARGE_FAULT_NONE != g_charge_fault);
    begin(); g_test_set_status = PLATFORM_ERR_HW;
    g_test_duty = g_charge_duty_permille = 600U; tick();
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_test_duty = g_charge_duty_permille = 951U;
    writes = g_test_writes; tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && writes == g_test_writes);
    assert(SVC_CHARGE_STATE_MPPT == g_charge_fault_state_snapshot);
    assert(951U == g_charge_fault_duty_snapshot && 0U == g_test_pwm);
    g_test_vin = 17000U;
    svc_charge_process(); assert(0U == g_charge_recovery_tick_count);
    for(i = 0U; i < 499U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    tick(); assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state && 1U == g_test_pwm);
    assert(17000U == g_charge_input_voc_mv && 14875U == g_charge_input_reference_mv);
    assert(794U == g_charge_duty_min_reference_permille && 804U == g_test_duty);
    begin(); g_test_stop_status = PLATFORM_ERR_HW;
    g_test_duty = g_charge_duty_permille = 951U; tick();
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault && 0U == g_charge_recovery_armed);
    g_test_stop_status = PLATFORM_ERR_OK; tick();
    assert(1U == g_charge_recovery_armed && 0U == g_test_pwm);
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    for(i = 0U; i < 600U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_OFF == g_charge_state && 0U == g_test_pwm);
    begin(); svc_charge_process(); svc_charge_process();
    assert(SVC_CHARGE_FAULT_SAMPLE_TIMEOUT == g_charge_fault && 0U == g_test_pwm);
    puts("PASS: OVP/OC/input/timeout, invalid Voc/range, PWM failure, snapshot, recovery/re-sample, stop");
}
int main(void)
{
    assert(PLATFORM_ERR_OK == svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_12V));
    test_ratio_helpers();
    test_input_and_clamp();
    test_cv_and_mppt();
    test_cv_input_constraint();
    test_faults_and_recovery();
    return 0;
}
'@
$readySource = @'
#include <assert.h>
#include <stdio.h>
#include "svc_measure.c"
void bsp_measure_set_callbacks(bsp_measure_data_callback_t pf_data,
                               bsp_measure_error_callback_t pf_error)
{ (void)pf_data; (void)pf_error; }
platform_err_t bsp_measure_init(void) { return PLATFORM_ERR_OK; }
void bsp_measure_process(void) {}
int main(void)
{
    svc_measure_value_t value = {0};
    unsigned int i;
    assert(256U == SVC_MEASURE_READY_FRAME_COUNT);
    assert(PLATFORM_ERR_OK == svc_measure_init());
    for(i = 1U; i < 256U; i++)
    {
        measure_data_ready(&value, i);
        assert(0U == svc_measure_is_ready());
    }
    measure_data_ready(&value, 256U);
    assert(1U == svc_measure_is_ready());
    measure_data_ready(&value, 258U);
    assert(0U == svc_measure_is_ready());
    for(i = 259U; i <= 513U; i++) { measure_data_ready(&value, i); }
    assert(1U == svc_measure_is_ready());
    measure_error(BSP_MEASURE_ERROR_DMA, PLATFORM_ERR_HW);
    assert(0U == svc_measure_is_ready());
    puts("PASS: startup requires 256 consecutive valid frames; gap/error resets");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $common = @('02_Service/svc_charge_diagnostics.c',
        '02_Service/svc_charge_config.c', '02_Service/svc_charge_control.c',
        '02_Service/svc_charge_cc.c', '02_Service/svc_charge_cv.c',
        '02_Service/svc_charge_maintenance.c', '02_Service/svc_charge_protection.c')
    $flags = @('-x','c','-std=c99','-Wall','-Wextra','-Werror',
        '-I02_Service','-I03_Platform/bsp','-I03_Platform/comm','-I03_Platform/mcu_interface')
    foreach ($mode in @(0, 1)) {
        $executable = Join-Path $testDirectory "guard-$mode.exe"
        $source | & $Gcc @flags "-DSVC_CHARGE_ENABLE_MAINTENANCE=$mode" -o $executable - @common
        if ($LASTEXITCODE -ne 0) { throw "Guard compile failed, mode=$mode" }
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "Guard assertions failed, mode=$mode" }
    }
    $executable = Join-Path $testDirectory 'ready.exe'
    $readySource | & $Gcc @flags -o $executable -
    if ($LASTEXITCODE -ne 0) { throw 'Readiness compile failed' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'Readiness assertions failed' }
    Write-Output "Host executables retained at $testDirectory"
} finally { Pop-Location }
