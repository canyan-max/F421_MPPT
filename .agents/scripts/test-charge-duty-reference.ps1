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
    assert(g_test_pwm);
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
static void begin(void)
{
    g_test_vin = 20000U; g_test_bat = 13500U; g_test_current = 700U;
    g_test_load = 0U; g_test_stop_status = PLATFORM_ERR_OK;
    assert(PLATFORM_ERR_OK == svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_12V));
    assert(PLATFORM_ERR_OK == svc_charge_start());
    tick();
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(675U + SVC_CHARGE_START_DUTY_OFFSET_PERMILLE == g_test_duty && 675U == g_charge_duty_min_reference_permille);
    tick(); tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
}
int main(void)
{
    svc_measure_value_t value = {0};
    uint16_t reference;
    uint16_t expected;
    uint16_t duty;
    uint32_t vin;
    uint32_t bat;
    uint32_t writes;
    unsigned int i;
    begin();
    value.input_voltage_mv = 20000U; value.battery_voltage_mv = 18900U;
    assert(PLATFORM_ERR_OK == svc_charge_control_calculate_initial_duty(&value, &duty));
    assert(950U == duty); /* Offset saturates; reference remains 945. */
    value.battery_voltage_mv = 19020U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_calculate_initial_duty(&value, &duty));
    /* Exhaust integer rounded boundary equivalence under changing inputs. */
    for(vin = 14001U; vin <= 48000U; vin += 997U)
    {
        for(bat = 10000U; bat < vin && bat <= 14500U; bat += 103U)
        {
            value.input_voltage_mv = vin;
            value.battery_voltage_mv = bat;
            assert(PLATFORM_ERR_OK ==
                svc_charge_control_calculate_duty_reference(&value, &expected));
            for(duty = 1U; duty <= 950U; duty++)
            {
                reference = 675U; /* Intentionally stale cache. */
                assert((duty >= expected) == (PLATFORM_ERR_OK ==
                    svc_charge_control_check_duty_reference(&value, duty, &reference)));
            }
        }
    }
    value.input_voltage_mv = 20000U; value.battery_voltage_mv = 13500U;
    reference = 675U;
    assert(PLATFORM_ERR_OK == svc_charge_control_check_duty_reference(&value, 675U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 674U, &reference));
    value.battery_voltage_mv = 0U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 1U, &reference));
    value.input_voltage_mv = 48000U; value.battery_voltage_mv = 5000U;
    reference = 675U;
    assert(PLATFORM_ERR_OK == svc_charge_control_check_duty_reference(&value, 104U, &reference));
    assert(104U == reference); /* No fixed 150 per-mille floor. */
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 103U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 0U, &reference));
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(NULL, 675U, &reference));
    value.input_voltage_mv = 0U;
    assert(PLATFORM_ERR_PARAM == svc_charge_control_check_duty_reference(&value, 675U, &reference));
    /* No unconditional recalculation while live and cached limits are safe. */
    g_test_vin = 21000U;
    tick();
    assert(675U == g_charge_duty_min_reference_permille);
    value.input_voltage_mv = g_test_vin; value.battery_voltage_mv = g_test_bat;
    assert(PLATFORM_ERR_OK == svc_charge_control_check_duty_reference(&value, 674U,
        &g_charge_duty_min_reference_permille));
    assert(643U == g_charge_duty_min_reference_permille);
    /* User report: applied D=reference=669, downward MPPT probe requests 668.
     * It must be canceled/rebaselined, not converted to a startup fault. */
    begin();
    g_test_bat = 13380U;
    g_charge_duty_permille = 669U;
    g_test_duty = 669U;
    g_charge_duty_min_reference_permille = 669U;
    svc_charge_mppt_reset();
    g_charge_mppt_direction_increase = 0U;
    writes = g_test_writes;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(669U == g_test_duty && 669U == g_charge_duty_permille);
    assert(1U == g_test_pwm && writes == g_test_writes);
    assert(SVC_CHARGE_MPPT_PHASE_REACQUIRE == svc_charge_mppt_get_phase());
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(670U == g_test_duty && SVC_CHARGE_STATE_MPPT == g_charge_state);
    /* A downward step that reaches, but does not cross, the bound is legal. */
    begin();
    g_test_bat = 13380U;
    g_charge_duty_permille = 670U;
    g_test_duty = 670U;
    g_charge_duty_min_reference_permille = 669U;
    svc_charge_mppt_reset();
    g_charge_mppt_direction_increase = 0U;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(669U == g_test_duty && 1U == g_test_pwm);
    begin();
    writes = g_test_writes;
    g_test_vin = 19000U; /* Applied D below the live ratio: hold, not fault. */
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(711U == g_charge_duty_min_reference_permille);
    assert(1U == g_test_pwm && writes == g_test_writes);
    for(i = 0U; i < 100U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 1U == g_test_pwm);
    assert(g_test_duty > 675U && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* Reported failure frame: rounded reference 733 and applied duty 732. */
    begin();
    g_test_vin = 19302U; g_test_bat = 14150U; g_test_current = 597U;
    g_charge_duty_permille = 732U; g_test_duty = 732U;
    svc_charge_mppt_reset();
    g_charge_mppt_direction_increase = 0U;
    writes = g_test_writes;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(733U == g_charge_duty_min_reference_permille);
    assert(732U == g_test_duty && writes == g_test_writes);
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 1U == g_test_pwm);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(733U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* Upward recovery is legal even before it reaches a rising reference. */
    begin();
    g_test_vin = 19302U; g_test_bat = 14150U; g_test_current = 597U;
    g_charge_duty_permille = 728U; g_test_duty = 728U;
    svc_charge_mppt_resume(728U);
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(729U == g_test_duty && 1U == g_test_pwm);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* Encoding errors and invalid voltage relations are not soft bounds. */
    begin();
    g_charge_duty_permille = 951U; g_test_duty = 951U; tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_test_vin = g_test_bat; tick();
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault && 0U == g_test_pwm);
    /* CC retains its existing applied-duty guard. */
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
    g_test_vin = 19000U; tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    for(i = 0U; i < 500U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_WAIT_SAMPLE == g_charge_state);
    tick();
    assert(711U + SVC_CHARGE_START_DUTY_OFFSET_PERMILLE == g_test_duty && 1U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_CURRENT;
    g_charge_duty_permille = 675U; g_test_duty = 675U;
    g_test_current = 10100U;
    writes = g_test_writes;
    tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault);
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state && 0U == g_test_pwm);
    assert(writes == g_test_writes);
    /* Enter CV through its real entry function, then drop Vin.
     * Increase must pass the old entry duty and recover below a live ratio. */
    begin();
    g_test_vin = 18000U; g_test_bat = 14400U;
    g_charge_duty_permille = 800U; g_test_duty = 800U;
    value.input_voltage_mv = g_test_vin; value.battery_voltage_mv = g_test_bat;
    enter_constant_voltage(&value);
    g_test_vin = 17000U; g_test_bat = 14370U;
    writes = g_test_writes;
    for(i = 0U; i < 50U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault && 1U == g_test_pwm);
    assert(850U == g_test_duty && writes + 50U == g_test_writes);
    /* Rising Vin lowers the reference and permits ordinary downward CV. */
    g_test_vin = 20000U; g_test_bat = 14450U;
    tick();
    assert(849U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14414U; g_test_vin = 19922U; g_test_current = 638U;
    g_charge_duty_permille = 723U; g_test_duty = 723U;
    writes = g_test_writes;
    for(i = 0U; i < 100U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(724U == g_charge_duty_min_reference_permille);
    assert(723U == g_test_duty && writes == g_test_writes);
    g_test_bat = 14370U; /* Upward recovery allowed even while below reference. */
    tick();
    assert(724U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    g_test_vin = 20000U; g_test_bat = 14450U;
    tick(); /* Equality is legal. */
    assert(723U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(0U == g_charge_cv_observation.downward_blocked);
    writes = g_test_writes;
    for(i = 0U; i < SVC_CHARGE_CV_DUTY_REFERENCE_MARGIN_PERMILLE; i++) { tick(); }
    assert(708U == g_test_duty && writes + 15U == g_test_writes);
    assert(708U == g_charge_cv_observation.duty_min_permille);
    writes = g_test_writes;
    tick(); /* Below the adjusted floor: retain CV and PWM. */
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(708U == g_charge_duty_permille && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(writes == g_test_writes);
    g_test_load = 1000U;
    for(i = 0U; i < 600U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(708U == g_test_duty && writes == g_test_writes);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    assert(1U == g_charge_cv_observation.valid);
    assert(1U == g_charge_cv_observation.downward_blocked);
    assert(708U == g_charge_cv_observation.duty_permille);
    assert(723U == g_charge_cv_observation.duty_reference_permille);
    assert(15211U == g_charge_cv_observation.input_min_reference_mv);
    assert(0U == g_charge_cv_observation.low_current);
    assert(0U == g_charge_cv_observation.low_power_boundary);
    g_test_load = 0U;
    g_test_current = 101U; tick();
    assert(0U == g_charge_cv_observation.low_current);
    g_test_current = 100U; tick();
    assert(1U == g_charge_cv_observation.low_current);
    assert(1U == g_charge_cv_observation.near_reference);
    assert(1U == g_charge_cv_observation.low_power_boundary);
    assert(708U == g_test_duty && writes == g_test_writes && 1U == g_test_pwm);
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state);
    g_test_current = 1500U; g_test_load = 1500U; tick();
    assert(1500U == g_charge_cv_observation.total_output_current_ma);
    assert(0U == g_charge_cv_observation.low_current); /* Net battery = 0. */
    g_test_current = 100U; g_test_load = 0U;
    g_charge_duty_permille = 730U; g_test_duty = 730U; tick();
    assert(0U == g_charge_cv_observation.near_reference);
    assert(0U == g_charge_cv_observation.low_power_boundary);
    assert(1U == g_charge_cv_observation.low_current);
    assert(729U == g_test_duty); /* Observation does not suppress legal CV. */
    g_charge_duty_permille = 950U; g_test_duty = 950U;
    g_test_vin = 15211U; tick();
    assert(1U == g_charge_cv_observation.input_headroom_low);
    assert(949U == g_test_duty && 1U == g_test_pwm);
    g_test_vin = 15212U; tick();
    assert(0U == g_charge_cv_observation.input_headroom_low);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* Reported frame: reference 725, floor 710. Equality is legal. */
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_vin = 20005U; g_test_bat = 14504U; g_test_current = 385U;
    g_charge_duty_permille = 718U; g_test_duty = 718U;
    tick();
    assert(717U == g_test_duty);
    assert(725U == g_charge_cv_observation.duty_reference_permille);
    assert(710U == g_charge_cv_observation.duty_min_permille);
    for(i = 0U; i < 7U; i++) { tick(); }
    assert(710U == g_test_duty);
    writes = g_test_writes;
    tick();
    assert(710U == g_test_duty && writes == g_test_writes);
    assert(1U == g_charge_cv_observation.downward_blocked);
    assert(1U == get_cv_duty_min(0U));
    assert(1U == get_cv_duty_min(15U));
    assert(1U == get_cv_duty_min(16U));
    assert(2U == get_cv_duty_min(17U));
    /* A drifting reference does not force D up; recovery below floor passes. */
    g_test_vin = 18000U; g_test_bat = 14370U;
    tick();
    assert(711U == g_test_duty && SVC_CHARGE_FAULT_NONE == g_charge_fault);
    /* Voltage below CV exit uses the original CC/MPPT handoff. */
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_vin = 20000U; g_test_bat = 14300U;
    g_charge_duty_permille = 800U; g_test_duty = 800U;
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state && 1U == g_test_pwm);
    /* True faults still shut down in CV. */
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14600U;
    tick();
    assert(SVC_CHARGE_FAULT_BATTERY_OVERVOLTAGE == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14414U; g_test_vin = 19922U;
    g_charge_duty_permille = 723U; g_test_duty = 723U; g_test_current = 12000U;
    tick();
    assert(SVC_CHARGE_FAULT_CHARGE_OVERCURRENT == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_charge_duty_permille = 951U; g_test_duty = 951U;
    tick();
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14600U;
    g_test_stop_status = PLATFORM_ERR_HW;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_POWER_PATH == g_charge_fault && 1U == g_test_pwm);
    g_test_stop_status = PLATFORM_ERR_OK;
    tick(); assert(0U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    svc_charge_process(); svc_charge_process();
    assert(SVC_CHARGE_FAULT_SAMPLE_TIMEOUT == g_charge_fault && 0U == g_test_pwm);
    begin();
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_bat = 14370U; g_test_vin = 17000U; g_test_current = 0U;
    g_charge_duty_permille = 950U; g_test_duty = 950U;
    for(i = 0U; i < 20U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state && 0U == g_test_pwm);
    assert(SVC_CHARGE_FAULT_INPUT_VOLTAGE == g_charge_fault);
    /* 24 V downward reference crossing also holds CV/PWM without a new state. */
    assert(PLATFORM_ERR_OK == svc_charge_init(SVC_CHARGE_BATTERY_SYSTEM_24V));
    g_charge_state = SVC_CHARGE_STATE_CONSTANT_VOLTAGE;
    g_test_vin = 40000U; g_test_bat = 28850U; g_test_current = 700U;
    g_charge_duty_permille = 721U; g_test_duty = 721U; g_test_pwm = 1U;
    writes = g_test_writes;
    for(i = 0U; i < 100U; i++) { tick(); }
    assert(SVC_CHARGE_STATE_CONSTANT_VOLTAGE == g_charge_state && 1U == g_test_pwm);
    assert(706U == g_test_duty && writes + 15U == g_test_writes);
    assert(SVC_CHARGE_FAULT_NONE == g_charge_fault);
    begin();
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    assert(0U == g_charge_cv_observation.valid);
    assert(PLATFORM_ERR_OK == svc_charge_start());
    tick();
    g_test_bat = 13550U;
    tick(); /* Settling may recompute startup D before checking that new D. */
    assert(SVC_CHARGE_STATE_START_SETTLE == g_charge_state);
    assert(678U + SVC_CHARGE_START_DUTY_OFFSET_PERMILLE == g_test_duty);
    tick();
    assert(SVC_CHARGE_STATE_MPPT == g_charge_state);
    assert(PLATFORM_ERR_OK == svc_charge_stop());
    assert(0U == g_charge_cv_observation.valid);
    assert(PLATFORM_ERR_OK == svc_charge_start());
    g_test_bat = 0U;
    writes = g_test_writes;
    tick();
    assert(SVC_CHARGE_STATE_FAULT == g_charge_state);
    assert(SVC_CHARGE_FAULT_DUTY_LIMIT == g_charge_fault);
    assert(0U == g_test_pwm && writes == g_test_writes);
    puts("PASS: rounded dynamic guard over input/battery/PWM ranges, stale cache,");
    puts("      MPPT drift hold/upward recovery, CC guard, CV hold/OVP/OC");
    puts("      100 mA observation/headroom, preserved control/startup, missing Vbat OFF");
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
    $common = @('02_Service/svc_charge_config.c', '02_Service/svc_charge_control.c',
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
