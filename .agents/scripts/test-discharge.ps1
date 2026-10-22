param(
    [string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe',
    [string]$BaselineSource = ''
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('discharge-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "svc_discharge.c"
#define TEST_TRIP_TICKS (2000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)
#define TEST_RECOVER_TICKS (5000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)
#define TEST_OC_RECOVER_TICKS (10000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)
#define TEST_SHORT_RETRY_TICKS (30000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)

static svc_measure_value_t g_test_value;
static uint32_t g_test_sequence;
static uint8_t g_test_ready;
static uint8_t g_test_output;
static uint32_t g_test_writes;
static platform_err_t g_test_measure_status;
static platform_err_t g_test_gpio_status;
static platform_err_t g_test_bind_status;

platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    *p_value = g_test_value;
    return g_test_measure_status;
}
uint8_t svc_measure_is_ready(void) { return g_test_ready; }
uint32_t svc_measure_get_sequence(void) { return g_test_sequence; }
platform_err_t bsp_power_path_discharge_init(void) { return g_test_bind_status; }
bsp_discharge_short_state_t bsp_power_path_discharge_short_get(void)
{
    bsp_discharge_short_state_t state = {0U, 0U};
    return state;
}
platform_err_t bsp_power_path_discharge_short_rearm(uint32_t generation)
{
    (void)generation;
    return PLATFORM_ERR_OK;
}
platform_err_t bsp_power_path_discharge_set(uint8_t enabled)
{
    assert(enabled <= 1U);
    g_test_writes++;
    if(PLATFORM_ERR_OK == g_test_gpio_status) { g_test_output = enabled; }
    return g_test_gpio_status;
}
static void ticks(unsigned int count)
{
    unsigned int i;
    for(i = 0U; i < count; i++)
    {
        g_test_sequence++;
        svc_discharge_process();
#ifdef DISCHARGE_TRACE_STATE
        printf("%u,%u,%u,%u,%u,%u,%u\n", (unsigned int)g_discharge_state,
               (unsigned int)g_discharge_fault_flags,
               (unsigned int)g_discharge_status,
               (unsigned int)g_discharge_manual_enabled,
               (unsigned int)g_discharge_output_enabled,
               (unsigned int)g_test_output, (unsigned int)g_test_writes);
#endif
        if(0U != g_discharge_fault_flags)
        {
            assert(0U == g_test_output ||
                   PLATFORM_ERR_OK != g_test_gpio_status);
        }
    }
}
static void begin_test(svc_discharge_battery_system_t system)
{
    memset(&g_test_value, 0, sizeof(g_test_value));
    /* No Vin and no charge current: discharge must remain independent. */
    g_test_value.battery_voltage_mv =
        SVC_DISCHARGE_BATTERY_SYSTEM_12V == system ? 13000U : 26000U;
    g_test_value.temperature_deci_c = 250;
    g_test_value.output_current_ma = 1000U;
    g_test_ready = 1U;
    g_test_measure_status = PLATFORM_ERR_OK;
    g_test_gpio_status = PLATFORM_ERR_OK;
    g_test_bind_status = PLATFORM_ERR_OK;
    assert(PLATFORM_ERR_OK == svc_discharge_init(system));
    assert(1U == g_discharge_manual_enabled);
    assert(0U == g_test_output);
    ticks(TEST_RECOVER_TICKS - 1U);
    assert(0U == g_test_output);
    ticks(1U);
    assert(1U == g_test_output);
    assert(SVC_DISCHARGE_STATE_ON == g_discharge_state);
}
static void assert_trip(uint32_t flag)
{
    ticks(TEST_TRIP_TICKS - 1U);
    assert(1U == g_test_output);
    ticks(1U);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & flag));
}
static void assert_recover(uint32_t flag)
{
    ticks(SVC_DISCHARGE_FAULT_OVERCURRENT == flag ? TEST_OC_RECOVER_TICKS - 1U : TEST_RECOVER_TICKS - 1U);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & flag));
    ticks(1U);
    assert(0U == (g_discharge_fault_flags & flag));
    assert(1U == g_test_output);
}
int main(void)
{
    unsigned int i;
    uint32_t writes;
    const svc_discharge_config_t *p_config;

    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    p_config = svc_discharge_get_config();
    assert(-400 == p_config->low_trip_deci_c);
    assert(-300 == p_config->low_recover_deci_c);
    assert(750 == p_config->high_trip_deci_c);
    assert(650 == p_config->high_recover_deci_c);
    assert(9000U == p_config->current_trip_ma);
    assert(200U == SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(TEST_TRIP_TICKS == p_config->trip_ticks);
    assert(TEST_RECOVER_TICKS == p_config->recover_ticks);
    assert(TEST_OC_RECOVER_TICKS == p_config->overcurrent_recover_ticks);
    assert(TEST_SHORT_RETRY_TICKS == p_config->short_retry_ticks);
    assert(TEST_TRIP_TICKS == p_config->sample_timeout_ticks);
    writes = g_test_writes;
    ticks(TEST_OC_RECOVER_TICKS);
    assert(writes == g_test_writes); /* No GPIO spam when state is unchanged. */
    g_test_value.temperature_deci_c = -400;
    assert_trip(SVC_DISCHARGE_FAULT_TEMPERATURE_LOW);
    g_test_value.temperature_deci_c = -301;
    ticks(6000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(0U == g_test_output);
    g_test_value.temperature_deci_c = -300;
    assert_recover(SVC_DISCHARGE_FAULT_TEMPERATURE_LOW);
    g_test_value.temperature_deci_c = 750;
    assert_trip(SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH);
    g_test_value.temperature_deci_c = 651;
    ticks(6000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(0U == g_test_output);
    g_test_value.temperature_deci_c = 650;
    assert_recover(SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH);

    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    g_test_value.battery_voltage_mv = 10000U;
    assert_trip(SVC_DISCHARGE_FAULT_BATTERY_LOW);
    g_test_value.battery_voltage_mv = 11999U;
    ticks(6000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(0U == g_test_output);
    g_test_value.battery_voltage_mv = 12000U;
    assert_recover(SVC_DISCHARGE_FAULT_BATTERY_LOW);
    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_24V);
    g_test_value.battery_voltage_mv = 20000U;
    assert_trip(SVC_DISCHARGE_FAULT_BATTERY_LOW);
    g_test_value.battery_voltage_mv = 23999U;
    ticks(6000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(0U == g_test_output);
    g_test_value.battery_voltage_mv = 24000U;
    assert_recover(SVC_DISCHARGE_FAULT_BATTERY_LOW);

    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    g_test_value.output_current_ma = 9000U;
    assert_trip(SVC_DISCHARGE_FAULT_OVERCURRENT);
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    ticks(1U);
    assert(0U == g_test_output); /* Manual ON cannot clear a latched fault. */
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    g_test_value.output_current_ma = 8999U;
    ticks(TEST_OC_RECOVER_TICKS - 1U);
    assert(0U == g_test_output);
    g_test_value.output_current_ma = 9000U;
    ticks(1U); /* Recovery must restart after another overcurrent sample. */
    g_test_value.output_current_ma = 8999U;
    assert_recover(SVC_DISCHARGE_FAULT_OVERCURRENT);
    g_test_value.input_current_ma = 12000U;
    g_test_value.output_current_ma = 1000U;
    ticks(3000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert(1U == g_test_output); /* IN_CUR is not the load overcurrent input. */

    /* Two protections must latch/recover independently, not reset each other. */
    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    g_test_value.temperature_deci_c = 750;
    g_test_value.battery_voltage_mv = 10000U;
    assert_trip(SVC_DISCHARGE_FAULT_BATTERY_LOW);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH));
    g_test_value.battery_voltage_mv = 13000U;
    ticks(TEST_RECOVER_TICKS);
    assert(0U == (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_BATTERY_LOW));
    assert(0U == g_test_output);
    g_test_value.temperature_deci_c = 650;
    ticks(TEST_RECOVER_TICKS - 1U);
    g_test_value.temperature_deci_c = 651;
    ticks(1U);
    g_test_value.temperature_deci_c = 650;
    assert_recover(SVC_DISCHARGE_FAULT_TEMPERATURE_HIGH);

    /* A sub-2 s excursion cannot latch, and a good sample resets its timer. */
    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    g_test_value.output_current_ma = 9000U;
    ticks(TEST_TRIP_TICKS - 1U);
    g_test_value.output_current_ma = 1000U;
    ticks(1U);
    g_test_value.output_current_ma = 9000U;
    assert_trip(SVC_DISCHARGE_FAULT_OVERCURRENT);
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    g_test_value.output_current_ma = 0U;
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    ticks(1U);
    assert(1U == g_test_output);
    g_discharge_manual_enabled = 0U;
    ticks(1U);
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_PARAM == svc_discharge_set_manual_enabled(2U));

    /* ADC/NTC errors shut off immediately; healthy recovery needs 5 s of new samples. */
    begin_test(SVC_DISCHARGE_BATTERY_SYSTEM_12V);
    g_test_value.temperature_deci_c = SVC_MEASURE_TEMPERATURE_INVALID;
    ticks(1U);
    assert(0U == g_test_output);
    g_test_value.temperature_deci_c = 250;
    assert_recover(SVC_DISCHARGE_FAULT_MEASUREMENT);
    g_test_measure_status = PLATFORM_ERR_HW;
    ticks(1U);
    assert(0U == g_test_output);
    g_test_measure_status = PLATFORM_ERR_OK;
    assert_recover(SVC_DISCHARGE_FAULT_MEASUREMENT);
    g_test_ready = 0U;
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    g_test_ready = 1U;
    assert_recover(SVC_DISCHARGE_FAULT_MEASUREMENT);

    /* Old samples neither recover faults nor count toward confirmation. */
    for(i = 0U; i < TEST_TRIP_TICKS - 1U; i++) { svc_discharge_process(); }
    assert(1U == g_test_output);
    svc_discharge_process();
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_TIMEOUT == g_discharge_status);
    assert_recover(SVC_DISCHARGE_FAULT_MEASUREMENT);
    g_test_sequence = UINT32_MAX;
    svc_discharge_process();
    ticks(1U); /* Sequence wrap must still be recognized as new. */
    assert(1U == g_test_output);

    /* Failed GPIO OFF is retried and never falsely reported as confirmed OFF. */
    g_test_gpio_status = PLATFORM_ERR_HW;
    assert(PLATFORM_ERR_HW == svc_discharge_set_manual_enabled(0U));
    assert(1U == g_discharge_output_enabled);
    writes = g_test_writes;
    ticks(2U);
    assert(g_test_writes > writes);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_GPIO));
    g_test_gpio_status = PLATFORM_ERR_OK;
    ticks(TEST_RECOVER_TICKS);
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    ticks(1U);
    assert(1U == g_test_output);

    /* Initialization cannot open into bad conditions or an invalid profile. */
    assert(PLATFORM_ERR_OK == svc_discharge_init(SVC_DISCHARGE_BATTERY_SYSTEM_12V));
    g_test_value.battery_voltage_mv = 9000U;
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_PARAM == svc_discharge_init((svc_discharge_battery_system_t)-1));
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_CONFIG));
    g_test_bind_status = PLATFORM_ERR_HW;
    assert(PLATFORM_ERR_HW == svc_discharge_init(SVC_DISCHARGE_BATTERY_SYSTEM_12V));
    ticks(TEST_OC_RECOVER_TICKS);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_CONFIG));
    puts("PASS: 200 ms discharge period, separate 12/24 V profiles, exact thresholds,");
    puts("      2 s trips, 5 s/10 s recoveries, simultaneous faults, manual permit,");
    puts("      sequence wrap, GPIO failure retries, Vin/charge independence");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $executable = Join-Path $testDirectory 'test.exe'
    $traceArgs = @()
    if ($BaselineSource) { $traceArgs = @('-DDISCHARGE_TRACE_STATE') }
    $source | & $Gcc -x c -std=c99 -Wall -Wextra -Werror -I02_Service `
        -I03_Platform/bsp -I03_Platform/comm -I03_Platform/mcu_interface `
        @traceArgs -o $executable - 02_Service/svc_discharge_config.c `
        02_Service/svc_discharge_protection.c
    if ($LASTEXITCODE -ne 0) { throw 'Discharge host compile failed' }
    $currentTrace = @(& $executable)
    if ($LASTEXITCODE -ne 0) { throw 'Discharge assertions failed' }
    if ($BaselineSource) {
        $baselineExecutable = Join-Path $testDirectory 'baseline.exe'
        $source.Replace('#include "svc_discharge.c"', $BaselineSource) | `
            & $Gcc -x c -std=c99 -Wall -Wextra -Werror -I02_Service `
            -I03_Platform/bsp -I03_Platform/comm -I03_Platform/mcu_interface `
            @traceArgs -o $baselineExecutable - 02_Service/svc_discharge_config.c `
            02_Service/svc_discharge_protection.c
        if ($LASTEXITCODE -ne 0) { throw 'Baseline host compile failed' }
        $baselineTrace = @(& $baselineExecutable)
        if ($LASTEXITCODE -ne 0) { throw 'Baseline assertions failed' }
        if ($currentTrace.Count -ne $baselineTrace.Count) { throw 'Trace length differs' }
        for ($index = 0; $index -lt $currentTrace.Count; $index++) {
            if ($currentTrace[$index] -cne $baselineTrace[$index]) {
                throw "Discharge behavior differs at trace line $index"
            }
        }
        Write-Output "PASS: baseline/refactor identical, $($currentTrace.Count - 3) task snapshots"
    } else { $currentTrace }
    Write-Output "Host executable retained at $testDirectory"
} finally { Pop-Location }
