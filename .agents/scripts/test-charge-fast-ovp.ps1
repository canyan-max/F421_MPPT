param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('fast-ovp-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include "bsp_measure.c"
static uint8_t g_test_pwm = 1U;
static unsigned int g_test_off_count;
static platform_err_t g_test_off_status;
platform_err_t plat_pwm_outputs_off(void)
{
    g_test_off_count++;
    if(PLATFORM_ERR_OK == g_test_off_status) { g_test_pwm = 0U; }
    return g_test_off_status;
}
void plat_adc_memory_barrier(void) {}
platform_err_t plat_adc_start(plat_adc_id_t id, volatile uint16_t *p_buffer,
                              uint16_t count, plat_adc_callback_t pf_callback)
{
    (void)id; (void)p_buffer; (void)count; (void)pf_callback;
    return PLATFORM_ERR_OK;
}
int main(void)
{
    uint16_t samples[BSP_MEASURE_CHANNEL_COUNT] = {0U};
    samples[BSP_MEASURE_BAT_VOL] = BOARD_MEASURE_BAT_FAST_OVP_RAW;
    bsp_measure_fast_ovp_enable(0U);
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_HALF,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(0U == g_test_off_count && 1U == g_test_pwm);
    bsp_measure_fast_ovp_enable(1U);
    samples[BSP_MEASURE_BAT_VOL] = (BOARD_MEASURE_BAT_FAST_OVP_RAW - 1U);
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_HALF,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(0U == g_test_off_count);
    samples[BSP_MEASURE_BAT_VOL] = BOARD_MEASURE_BAT_FAST_OVP_RAW;
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_FULL,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(1U == g_test_off_count && 0U == g_test_pwm);
    assert(BOARD_MEASURE_BAT_FAST_OVP_RAW == g_charge_fast_ovp_trip_raw);
    assert(PLATFORM_ERR_OK == g_charge_fast_ovp_off_status);
    assert(BOARD_MEASURE_BAT_FAST_OVP_RAW == g_adc_raw[BSP_MEASURE_BAT_VOL]);
    samples[BSP_MEASURE_BAT_VOL] = 1700U;
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_HALF,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(1U == g_test_off_count && 0U == g_test_pwm);
    g_test_pwm = 1U; /* No latch: a later explicit start is not interlocked. */
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_HALF,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(1U == g_test_pwm && 1U == g_test_off_count);
    samples[BSP_MEASURE_BAT_VOL] = 1828U;
    g_test_off_status = PLATFORM_ERR_HW;
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_FULL,
                         samples, BSP_MEASURE_CHANNEL_COUNT);
    assert(PLATFORM_ERR_HW == g_charge_fast_ovp_off_status);
    assert(1828U == g_charge_fast_ovp_trip_raw);
    measure_adc_callback(BOARD_ADC_MEASURE, PLAT_ADC_EVENT_ERROR, NULL, 0U);
    assert(2U == g_test_off_count);
    puts("PASS: DMA configured raw threshold boundary, direct PWM OFF, diagnostic result,");
    puts("      continued raw publication, no automatic ON/latch, disabled check");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $executable = Join-Path $testDirectory 'fast-ovp.exe'
    $flags = @('-x','c','-std=c99','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
        '-I00_Project_Config','-I03_Platform/bsp','-I03_Platform/mcu_interface',
        '-I06_Component/filter','-I03_Platform/comm')
    $source | & $Gcc @flags -o $executable - '06_Component/filter/filter.c'
    if ($LASTEXITCODE -ne 0) { throw 'Fast OVP host compile failed' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'Fast OVP assertions failed' }
    Write-Output "Host executable retained at $testDirectory"
} finally { Pop-Location }
