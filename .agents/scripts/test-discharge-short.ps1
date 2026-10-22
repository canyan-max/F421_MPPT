param(
    [string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe',
    [string]$BaselineSource = ''
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('discharge-short-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdio.h>
#include "svc_discharge.h"
#include "svc_measure.h"
#include "bsp_power_path.h"
#include "plat_gpio.h"
#include "plat_pwm.h"
#include "plat_sys.h"
#include "board_resources.h"
#define TEST_RECOVER_TICKS (5000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)
#define TEST_SHORT_RETRY_TICKS (30000U / SVC_DISCHARGE_PROCESS_PERIOD_MS)

static svc_measure_value_t g_test_value;
static uint32_t g_test_sequence;
static uint32_t g_test_mask;
static uint8_t g_test_output;
static uint8_t g_test_active;
static uint8_t g_test_pending;
static uint8_t g_test_inject_write;
static uint8_t g_test_inject_after_snapshot;
static uint8_t g_test_fresh = 1U;
static uint8_t g_test_ready = 1U;
static uint32_t g_test_writes;

/* Substitute registers/vendor primitives only; execute the real GPIO adapter.
 * These pin bindings mirror the generated board header checked by this task. */
#define __AT32F421_WK_CONFIG_H
typedef struct { unsigned int unused; } gpio_type;
typedef struct { uint32_t inten; } exint_type;
static gpio_type g_test_port_b;
static gpio_type g_test_port_f;
static exint_type g_test_exint;
#define EXINT (&g_test_exint)
#define EXINT_LINE_0 (1U)
#define RESET (0U)
#define FALSE (0U)
#define TRUE (1U)
#define LED1_PB3_GPIO_PORT (&g_test_port_b)
#define LED1_PB3_PIN (1U << 3U)
#define LED2_PB4_GPIO_PORT (&g_test_port_b)
#define LED2_PB4_PIN (1U << 4U)
#define LED3_PB5_GPIO_PORT (&g_test_port_b)
#define LED3_PB5_PIN (1U << 5U)
#define SOL_MOS_OUT_PB12_GPIO_PORT (&g_test_port_b)
#define SOL_MOS_OUT_PB12_PIN (1U << 12U)
#define LOADMOS_OUT_PB11_GPIO_PORT (&g_test_port_b)
#define LOADMOS_OUT_PB11_PIN (1U << 11U)
#define BT_VDD_EN_PB10_GPIO_PORT (&g_test_port_b)
#define BT_VDD_EN_PB10_PIN (1U << 10U)
#define RS485_TXEN_PF7_GPIO_PORT (&g_test_port_f)
#define RS485_TXEN_PF7_PIN (1U << 7U)
#define OUT_CUR_ERR_GPIO_PORT (&g_test_port_b)
#define OUT_CUR_ERR_PIN (1U)

static void dispatch(void)
{
    if(0U == g_test_mask && 0U != g_test_pending &&
       0U != (EXINT->inten & EXINT_LINE_0))
    {
        plat_gpio_irq_handler(BOARD_GPIO_LOAD_FAULT);
    }
}
uint32_t plat_irq_save(void)
{
    uint32_t mask = g_test_mask;
    g_test_mask = 1U;
    return mask;
}
void plat_irq_restore(uint32_t mask)
{
    assert(mask <= 1U);
    g_test_mask = mask;
    dispatch();
}
static uint8_t exint_flag_get(uint32_t line)
{
    assert(EXINT_LINE_0 == line);
    return g_test_pending;
}
static uint8_t exint_interrupt_flag_get(uint32_t line)
{
    return exint_flag_get(line) && 0U != (EXINT->inten & line);
}
static void exint_flag_clear(uint32_t line)
{
    assert(EXINT_LINE_0 == line);
    assert(0U == g_test_output); /* Callback must request OFF before ACK. */
    g_test_pending = 0U;
}
static void exint_interrupt_enable(uint32_t line, uint8_t enabled)
{
    assert(EXINT_LINE_0 == line && FALSE == enabled);
    EXINT->inten &= ~line;
}
static uint8_t gpio_input_data_bit_read(gpio_type *p_port, uint16_t pin)
{
    assert(OUT_CUR_ERR_GPIO_PORT == p_port && OUT_CUR_ERR_PIN == pin);
    return 0U == g_test_active;
}
static void gpio_bits_toggle(gpio_type *p_port, uint16_t pin)
{
    (void)p_port; (void)pin;
    assert(0); /* No toggle is used by protected discharge. */
}
static void gpio_bits_write(gpio_type *p_port, uint16_t pin, uint8_t enabled)
{
    assert(LOADMOS_OUT_PB11_GPIO_PORT == p_port && LOADMOS_OUT_PB11_PIN == pin);
    g_test_writes++;
    g_test_output = enabled;
    if(0U != g_test_inject_write && 0U != g_test_output)
    {
        g_test_inject_write = 0U;
        g_test_pending = 1U; /* Pulse arrived with IRQs masked. */
        dispatch();
    }
}
#include "04_Impl/mcu_at32/at32f421_gpio_adapter.c"

static void unexpected_callback(plat_gpio_id_t id)
{
    (void)id; assert(0);
}
/* Link actual BSP without allowing discharge to call its charge functions. */
platform_err_t plat_pwm_outputs_off(void) { assert(0); return PLATFORM_ERR_HW; }
platform_err_t plat_pwm_start(uint16_t duty)
{ (void)duty; assert(0); return PLATFORM_ERR_HW; }
platform_err_t plat_pwm_set_duty_permille(uint16_t duty)
{ (void)duty; assert(0); return PLATFORM_ERR_HW; }

platform_err_t svc_measure_get(svc_measure_value_t *p_value)
{
    if(0U != g_test_inject_after_snapshot)
    {
        g_test_inject_after_snapshot = 0U;
        g_test_pending = 1U;
        dispatch(); /* ISR after snapshot but before main decides ON. */
    }
    if(0U != g_test_fresh) { g_test_sequence++; }
    *p_value = g_test_value;
    return PLATFORM_ERR_OK;
}
uint8_t svc_measure_is_ready(void) { return g_test_ready; }
uint32_t svc_measure_get_sequence(void) { return g_test_sequence; }
static void trace_state(void)
{
#ifdef DISCHARGE_TRACE_STATE
    printf("%u,%u,%u,%u,%u,%u,%u,%u\n", (unsigned int)g_discharge_state,
           (unsigned int)g_discharge_fault_flags,
           (unsigned int)g_discharge_manual_enabled,
           (unsigned int)g_discharge_output_enabled,
           (unsigned int)g_test_output, (unsigned int)g_test_writes,
           (unsigned int)g_test_mask, (unsigned int)g_test_pending);
#endif
}
static void ticks(unsigned int count)
{
    unsigned int index;
    for(index = 0U; index < count; index++)
    {
        svc_discharge_process();
        trace_state();
    }
}
static void pulse(void)
{
    g_test_pending = 1U;
    dispatch();
    assert(0U == g_test_output); /* No service call is needed to shut off. */
    trace_state();
}
static void assert_short(void)
{
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_SHORT_CIRCUIT));
    assert(0U == (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_GPIO));
}
static void finish_holdoff(void)
{
    ticks(TEST_SHORT_RETRY_TICKS - 1U);
    assert_short();
    ticks(1U);
    assert(1U == g_test_output);
}
int main(void)
{
    bsp_discharge_short_state_t state;
    bsp_discharge_short_state_t newer;
    uint8_t pending;
    plat_gpio_state_t level;

    assert(PLATFORM_ERR_PARAM == plat_gpio_write(BOARD_GPIO_LOAD_FAULT, PLAT_GPIO_SET));
    assert(PLATFORM_ERR_PARAM == plat_gpio_toggle(BOARD_GPIO_LOAD_FAULT));
    assert(PLATFORM_ERR_PARAM == plat_gpio_read(BOARD_GPIO_RESOURCE_NUM, &level));
    assert(PLATFORM_ERR_PARAM == plat_gpio_read(BOARD_GPIO_LOAD_FAULT, NULL));
    assert(PLATFORM_ERR_PARAM == plat_gpio_irq_bind(BOARD_GPIO_LED1, unexpected_callback));
    assert(PLATFORM_ERR_PARAM == plat_gpio_irq_bind(BOARD_GPIO_LOAD_FAULT, NULL));
    assert(PLATFORM_ERR_PARAM == plat_gpio_irq_get_pending(BOARD_GPIO_LED1, &pending));
    assert(PLATFORM_ERR_PARAM == plat_gpio_irq_get_pending(BOARD_GPIO_LOAD_FAULT, NULL));
    assert(0U == plat_gpio_irq_is_ready(BOARD_GPIO_LED1));
    assert(0U == plat_gpio_irq_is_ready(BOARD_GPIO_LOAD_FAULT));
    EXINT->inten = EXINT_LINE_0;
    assert(PLATFORM_ERR_BUSY == plat_gpio_irq_bind(BOARD_GPIO_LOAD_FAULT, unexpected_callback));
    g_test_pending = 1U;
    dispatch(); /* Unbound event masks the line; no callback can authorize ON. */
    assert(0U == EXINT->inten && 0U == g_test_pending && 0U == g_test_output);
    assert(PLATFORM_ERR_BUSY == bsp_power_path_discharge_set(1U));
    assert(0U == g_test_output); /* Fail closed until initialized. */
    assert(PLATFORM_ERR_OK == bsp_power_path_discharge_init());
    assert(0U == plat_gpio_irq_is_ready(BOARD_GPIO_LOAD_FAULT));
    assert(PLATFORM_ERR_BUSY == plat_gpio_irq_bind(BOARD_GPIO_LOAD_FAULT, unexpected_callback));
    assert(PLATFORM_ERR_PARAM == plat_gpio_irq_bind(BOARD_GPIO_LOAD_FAULT, NULL));
    assert(PLATFORM_ERR_BUSY == bsp_power_path_discharge_set(1U));
    EXINT->inten = EXINT_LINE_0; /* Generated EXINT initialization arms PB0. */
    assert(0U != plat_gpio_irq_is_ready(BOARD_GPIO_LOAD_FAULT));
    g_test_value.battery_voltage_mv = 13000U;
    g_test_value.temperature_deci_c = 250;
    g_test_value.output_current_ma = 1000U;
    g_test_active = 1U; /* Already low at boot, without any falling edge. */
    assert(PLATFORM_ERR_OK == svc_discharge_init(SVC_DISCHARGE_BATTERY_SYSTEM_12V));
    ticks(1U);
    assert_short();
    ticks(40000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    assert_short(); /* 30 s cannot override a still-active hardware input. */
    g_test_active = 0U;
    ticks(1U);
    assert(1U == g_test_output);

    pulse();
    ticks(1U); /* First observation starts a conservative 30 s holdoff. */
    assert_short();
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    finish_holdoff();
    pulse();
    ticks(1U);
    ticks(20000U / SVC_DISCHARGE_PROCESS_PERIOD_MS);
    pulse(); /* Each new edge restarts the full holdoff, even while latched. */
    ticks(1U);
    finish_holdoff();

    /* Fault between service snapshot and ON cannot reopen the switch. */
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    g_test_inject_after_snapshot = 1U;
    ticks(1U);
    assert_short();
    ticks(1U);
    finish_holdoff();

    /* Pending edge inside the atomic enable write also forces LOW. */
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    g_test_inject_write = 1U;
    ticks(1U);
    assert_short();
    ticks(1U);
    finish_holdoff();

    /* Stale samples or unready acquisition must never bypass the latch. */
    pulse();
    ticks(1U);
    g_test_fresh = 0U;
    ticks(TEST_SHORT_RETRY_TICKS);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_MEASUREMENT));
    g_test_fresh = 1U;
    ticks(TEST_RECOVER_TICKS);
    assert(1U == g_test_output);
    pulse();
    ticks(1U);
    g_test_ready = 0U;
    ticks(TEST_SHORT_RETRY_TICKS);
    assert(0U == g_test_output);
    g_test_ready = 1U;
    ticks(TEST_RECOVER_TICKS);
    assert(1U == g_test_output);

    /* Reinit cannot erase a latched short; manual OFF remains authoritative. */
    pulse();
    assert(PLATFORM_ERR_OK == svc_discharge_init(SVC_DISCHARGE_BATTERY_SYSTEM_12V));
    ticks(1U);
    assert_short();
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(0U));
    ticks(TEST_SHORT_RETRY_TICKS);
    assert(0U == g_test_output);
    assert(PLATFORM_ERR_OK == svc_discharge_set_manual_enabled(1U));
    ticks(1U);
    assert(1U == g_test_output);

    /* Other guards still veto ON after short timer expires. */
    pulse();
    ticks(1U);
    g_test_value.battery_voltage_mv = 10000U;
    ticks(TEST_SHORT_RETRY_TICKS);
    assert(0U == g_test_output);
    assert(0U != (g_discharge_fault_flags & SVC_DISCHARGE_FAULT_BATTERY_LOW));
    g_test_value.battery_voltage_mv = 13000U;
    ticks(TEST_RECOVER_TICKS);
    assert(1U == g_test_output);

    /* Save/restore preserves an already-masked caller; pending pulses veto ON.
     * Old generation cannot clear an event arriving after a snapshot. */
    g_test_mask = 1U;
    g_test_pending = 1U;
    plat_gpio_irq_handler(BOARD_GPIO_LED1);
    assert(1U == g_test_pending && 1U == g_test_output);
    assert(PLATFORM_ERR_OK == plat_gpio_irq_get_pending(BOARD_GPIO_LOAD_FAULT, &pending));
    assert(1U == pending && 1U == g_test_pending); /* Read never ACKs. */
    assert(PLATFORM_ERR_OK == bsp_power_path_discharge_init());
    assert(1U == g_test_mask && 1U == g_test_pending); /* Same-owner bind preserves it. */
    assert(PLATFORM_ERR_BUSY == bsp_power_path_discharge_set(1U));
    assert(1U == g_test_mask);
    assert(0U == g_test_output);
    state = bsp_power_path_discharge_short_get();
    assert(1U == g_test_mask && 0U != state.latched);
    assert(PLATFORM_ERR_BUSY == bsp_power_path_discharge_short_rearm(state.generation));
    assert(1U == g_test_mask);
    plat_irq_restore(0U); /* Delivers pending event, advances generation again. */
    newer = bsp_power_path_discharge_short_get();
    assert(newer.generation != state.generation);
    assert(PLATFORM_ERR_BUSY == bsp_power_path_discharge_short_rearm(state.generation));
    assert(PLATFORM_ERR_OK == bsp_power_path_discharge_short_rearm(newer.generation));
    assert(0U == g_test_mask);

    puts("PASS: actual Service+BSP+GPIO, synchronous IRQ callback OFF before ACK,");
    puts("      bind/ID/null/late/replacement/readiness gates, 30 s retry, boot/stuck low,");
    puts("      repeated edges, reinit/manual/UV/ADC interlocks, pending-edge races,");
    puts("      generation-safe rearm, exact interrupt-mask restoration");
    return 0;
}
'@
Push-Location $repoRoot
try {
    $executable = Join-Path $testDirectory 'test.exe'
    $traceArgs = @()
    if ($BaselineSource) { $traceArgs = @('-DDISCHARGE_TRACE_STATE') }
    $source | & $Gcc -std=c99 -Wall -Wextra -Werror -I02_Service `
        -I. -Iproject/inc -I00_Project_Config -I03_Platform/bsp -I03_Platform/comm `
        -I03_Platform/mcu_interface @traceArgs -x c - 02_Service/svc_discharge.c `
        02_Service/svc_discharge_config.c 02_Service/svc_discharge_protection.c `
        03_Platform/bsp/bsp_power_path.c -o $executable
    if ($LASTEXITCODE -ne 0) { throw 'Short-protection host compile failed' }
    $currentTrace = @(& $executable)
    if ($LASTEXITCODE -ne 0) { throw 'Short-protection assertions failed' }
    if ($BaselineSource) {
        $baselineExecutable = Join-Path $testDirectory 'baseline.exe'
        ($BaselineSource + "`n" + $source) | & $Gcc -std=c99 -Wall -Wextra -Werror `
            -I02_Service -I. -Iproject/inc -I00_Project_Config -I03_Platform/bsp `
            -I03_Platform/comm -I03_Platform/mcu_interface @traceArgs -x c - `
            02_Service/svc_discharge_config.c 02_Service/svc_discharge_protection.c `
            03_Platform/bsp/bsp_power_path.c -o $baselineExecutable
        if ($LASTEXITCODE -ne 0) { throw 'Baseline short host compile failed' }
        $baselineTrace = @(& $baselineExecutable)
        if ($LASTEXITCODE -ne 0) { throw 'Baseline short assertions failed' }
        if ($currentTrace.Count -ne $baselineTrace.Count) { throw 'Short trace length differs' }
        for ($index = 0; $index -lt $currentTrace.Count; $index++) {
            if ($currentTrace[$index] -cne $baselineTrace[$index]) {
                throw "Short behavior differs at trace line $index"
            }
        }
        Write-Output "PASS: baseline/refactor identical, $($currentTrace.Count - 4) short task/IRQ snapshots"
    } else { $currentTrace }
    Write-Output "Host executable retained at $testDirectory"
} finally { Pop-Location }
