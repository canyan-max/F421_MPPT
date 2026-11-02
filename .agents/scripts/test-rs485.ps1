param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('rs485-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$vendor = @'
#ifndef TEST_UART_VENDOR_H
#define TEST_UART_VENDOR_H
#include <stdint.h>
typedef struct { uint32_t sts; uint16_t dt; uint32_t enabled; } usart_type;
extern usart_type g_test_uart;
extern usart_type g_test_uart2;
#define USART1 (&g_test_uart)
#define USART2 (&g_test_uart2)
#define FALSE (0U)
#define TRUE (1U)
#define RESET (0U)
#define USART_PERR_FLAG (1U)
#define USART_FERR_FLAG (2U)
#define USART_NERR_FLAG (4U)
#define USART_ROERR_FLAG (8U)
#define USART_RDBF_FLAG (32U)
#define USART_TDC_FLAG (64U)
#define USART_TDBE_FLAG (128U)
#define USART_RDBF_INT USART_RDBF_FLAG
#define USART_TDC_INT USART_TDC_FLAG
#define USART_TDBE_INT USART_TDBE_FLAG
#define USART_ERR_INT (256U)
void usart_interrupt_enable(usart_type *, uint32_t, uint8_t);
void usart_flag_clear(usart_type *, uint32_t);
uint16_t usart_data_receive(usart_type *);
void usart_data_transmit(usart_type *, uint16_t);
uint8_t usart_interrupt_flag_get(usart_type *, uint32_t);
#endif
'@
$source = @'
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "at32f421_wk_config.h"
#include "bsp_rs485.h"
#include "plat_uart.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
usart_type g_test_uart;
usart_type g_test_uart2;
static uint32_t g_now, g_mask, g_reads, g_sent;
static uint32_t g_uart2_done, g_uart2_rx, g_uart2_error, g_uart2_sent;
static uint8_t g_wire[512];
static plat_gpio_state_t g_direction;
uint32_t plat_tick_get_ms(void) { return g_now; }
uint32_t plat_irq_save(void) { uint32_t old=g_mask; g_mask=1U; return old; }
void plat_irq_restore(uint32_t mask) { g_mask=mask; }
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state)
{ assert(id == BOARD_GPIO_RS485_TXEN);
  g_direction=state; return PLATFORM_ERR_OK; }
void usart_interrupt_enable(usart_type *p, uint32_t bit, uint8_t enable)
{ if(enable) p->enabled |= bit; else p->enabled &= ~bit; }
void usart_flag_clear(usart_type *p, uint32_t flags)
{ if(flags & 15U) p->sts &= ~(15U|USART_RDBF_FLAG); else p->sts &= ~flags; }
uint16_t usart_data_receive(usart_type *p)
{ g_reads++; p->sts &= ~(15U|USART_RDBF_FLAG); return p->dt; }
void usart_data_transmit(usart_type *p, uint16_t value)
{ if(p==USART1) { assert(g_sent < sizeof(g_wire)); g_wire[g_sent++]=(uint8_t)value; }
  else { assert(p==USART2); g_uart2_sent++; }
  p->sts &= ~(USART_TDC_FLAG|USART_TDBE_FLAG); }
uint8_t usart_interrupt_flag_get(usart_type *p, uint32_t bit)
{ return (p->enabled & bit) && (p->sts & bit); }
static void rx(uint8_t byte, uint32_t error)
{ g_test_uart.dt=byte; g_test_uart.sts |= USART_RDBF_FLAG|error;
  plat_uart_irq_handler(BOARD_UART_RS485); }
static void ticks(uint32_t count)
{ while(count--) { g_now++; bsp_rs485_tick_1ms(); } }
static void uart_flag(uint32_t flag)
{ g_test_uart.sts |= flag; plat_uart_irq_handler(BOARD_UART_RS485); }
static void uart2_event(plat_uart_id_t id, plat_uart_event_t event, uint32_t value)
{
    assert(id==BOARD_UART_DEBUG);
    if(event==PLAT_UART_EVENT_RX_BYTE) { assert(value==0x7BU); g_uart2_rx++; }
    else if(event==PLAT_UART_EVENT_RX_ERROR) {
        assert(value==PLAT_UART_ERROR_FRAME); g_uart2_error++;
    }
    else { assert(event==PLAT_UART_EVENT_TX_DONE); g_uart2_done++; }
}
int main(void)
{
    uint8_t data[256], reply[]={0x00U,0xA5U,0xFFU};
    uint16_t count;
    unsigned i;
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_IDLE);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_ERROR);
    assert(count==0U);
    assert(bsp_rs485_read_segment(NULL,sizeof(data),&count)==BSP_RS485_RX_PARAM);
    assert(plat_uart_start(255U,uart2_event)==PLATFORM_ERR_PARAM);
    assert(plat_uart_send_async(255U,reply,1U)==PLATFORM_ERR_PARAM);
    assert(plat_uart_abort_tx(255U)==PLATFORM_ERR_PARAM);
    plat_uart_irq_handler(255U);
    assert(!plat_uart_tx_is_busy(255U));
    assert(plat_uart_send_async(BOARD_UART_DEBUG,reply,1U)==PLATFORM_ERR_HW);
    assert(plat_uart_start(BOARD_UART_DEBUG,uart2_event)==PLATFORM_ERR_OK);
    assert(plat_uart_start(BOARD_UART_DEBUG,uart2_event)==PLATFORM_ERR_BUSY);
    g_test_uart2.dt=0x7BU; g_test_uart2.sts=USART_RDBF_FLAG;
    plat_uart_irq_handler(BOARD_UART_DEBUG);
    assert(g_uart2_rx==1U && !(g_test_uart2.sts & USART_RDBF_FLAG));
    g_test_uart2.sts=USART_FERR_FLAG|USART_RDBF_FLAG;
    plat_uart_irq_handler(BOARD_UART_DEBUG);
    assert(g_uart2_error==1U && g_uart2_rx==1U);
    assert(bsp_rs485_init() == PLATFORM_ERR_OK);
    assert(bsp_rs485_init()==PLATFORM_ERR_BUSY);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_IDLE);
    assert(g_direction == PLAT_GPIO_RESET);
    assert(!(g_test_uart.enabled & (USART_TDC_INT|USART_TDBE_INT)));
    rx(0x11U,0U); ticks(4U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_EMPTY);
    ticks(1U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==1U && data[0]==0x11U);
    /* Preserve endpoints while the next segment is already being received. */
    rx(0x22U,0U); ticks(5U); rx(0x33U,0U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==1U && data[0]==0x22U);
    assert(bsp_rs485_send_async(reply,sizeof(reply))==PLATFORM_ERR_BUSY);
    ticks(5U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==1U && data[0]==0x33U);
    for(i=0U;i<256U;i++) rx((uint8_t)i,0U);
    ticks(5U);
    assert(bsp_rs485_read_segment(data,255U,&count)==BSP_RS485_RX_BUFFER_SMALL);
    assert(count==256U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==256U);
    for(i=0U;i<256U;i++) assert(data[i]==(uint8_t)i);
    for(i=0U;i<257U;i++) rx((uint8_t)i,0U);
    ticks(5U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_DROPPED);
    assert(count==0U && g_rs485_stats.rx_overflows==1U);
    /* One register read per error event; damaged prefix is never delivered. */
    rx(1U,0U); i=g_reads; rx(2U,USART_FERR_FLAG|USART_ROERR_FLAG);
    assert(g_reads==i+1U); rx(3U,0U);
    ticks(5U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_DROPPED);
    assert(count==0U && g_rs485_stats.rx_errors==1U);
    rx(1U,USART_PERR_FLAG); rx(2U,USART_NERR_FLAG); ticks(5U);
    memset(data,0xA5,sizeof(data));
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_DROPPED);
    assert(count==0U);
    for(i=0U;i<sizeof(data);i++) assert(data[i]==0xA5U);
    for(i=0U;i<5U;i++) { rx((uint8_t)i,0U); ticks(5U); }
    assert(g_rs485_stats.rx_overflows==2U);
    for(i=0U;i<4U;i++) {
        assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
        assert(count==1U && data[0]==i);
    }
    rx(0x55U,0U); ticks(5U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==1U && data[0]==0x55U);
    g_now=UINT32_MAX-2U; rx(0x66U,0U); ticks(5U);
    assert(bsp_rs485_read_segment(data,sizeof(data),&count)==BSP_RS485_RX_OK);
    assert(count==1U && data[0]==0x66U);
    assert(bsp_rs485_send_async(reply,sizeof(reply))==PLATFORM_ERR_OK);
    assert(g_direction==PLAT_GPIO_SET && (bsp_rs485_tx_get_result()==BSP_RS485_TX_BUSY));
    assert(bsp_rs485_send_async(reply,1U)==PLATFORM_ERR_BUSY);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_BUSY);
    ticks(BSP_RS485_TX_SETUP_MS-1U); bsp_rs485_process(); assert(g_sent==0U);
    ticks(1U); bsp_rs485_process();
    /* UART2 may complete or abort while UART1 still owns its buffer/DE. */
    assert(plat_uart_send_async(BOARD_UART_DEBUG,reply,1U)==PLATFORM_ERR_OK);
    g_test_uart2.sts=USART_TDBE_FLAG|USART_TDC_FLAG;
    plat_uart_irq_handler(BOARD_UART_DEBUG);
    assert(g_uart2_done==0U && g_uart2_sent==1U);
    g_test_uart2.sts=USART_TDC_FLAG;
    plat_uart_irq_handler(BOARD_UART_DEBUG);
    assert(g_uart2_done==1U && !plat_uart_tx_is_busy(BOARD_UART_DEBUG));
    assert(plat_uart_tx_is_busy(BOARD_UART_RS485) && g_direction==PLAT_GPIO_SET);
    assert(g_rs485_stats.tx_segments==0U && g_sent==0U);
    assert(plat_uart_send_async(BOARD_UART_DEBUG,reply,1U)==PLATFORM_ERR_OK);
    assert(plat_uart_abort_tx(BOARD_UART_DEBUG)==PLATFORM_ERR_OK);
    assert(plat_uart_tx_is_busy(BOARD_UART_RS485));
    uart_flag(USART_TDC_FLAG|USART_TDBE_FLAG);
    assert(g_sent==1U && g_rs485_stats.tx_segments==0U);
    uart_flag(USART_TDBE_FLAG); uart_flag(USART_TDBE_FLAG);
    assert(g_sent==3U && !memcmp(g_wire,reply,sizeof(reply)));
    assert((bsp_rs485_tx_get_result()==BSP_RS485_TX_BUSY) && g_direction==PLAT_GPIO_SET);
    assert(!(g_test_uart.enabled & USART_TDBE_INT));
    uart_flag(USART_TDC_FLAG);
    assert((bsp_rs485_tx_get_result()!=BSP_RS485_TX_BUSY) && g_direction==PLAT_GPIO_RESET);
    assert(g_rs485_stats.tx_segments==1U);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_DONE);
    assert(bsp_rs485_send_async(NULL,1U)==PLATFORM_ERR_PARAM);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_DONE);
    assert(!(g_test_uart.enabled & (USART_TDC_INT|USART_TDBE_INT)));
    assert(bsp_rs485_send_async(reply,1U)==PLATFORM_ERR_OK);
    ticks(1U); bsp_rs485_process(); uart_flag(USART_TDBE_FLAG);
    ticks(30U); bsp_rs485_process();
    assert(g_rs485_stats.tx_timeouts==1U && g_direction==PLAT_GPIO_RESET);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_FAILED);
    assert(bsp_rs485_send_async(reply,1U)==PLATFORM_ERR_BUSY);
    ticks(5U);
    assert(bsp_rs485_send_async(reply,1U)==PLATFORM_ERR_OK);
    ticks(1U); bsp_rs485_process(); uart_flag(USART_TDBE_FLAG);
    uart_flag(USART_TDC_FLAG);
    assert(bsp_rs485_tx_get_result()==BSP_RS485_TX_DONE);
    assert(!g_mask);
    puts("UART IDs/isolation and RS485 gap/endpoints, 256-byte/wrap, overflow/error, TDC and timeout passed.");
    return 0;
}
'@
Set-Content -LiteralPath (Join-Path $testDirectory 'at32f421_wk_config.h') -Value $vendor -Encoding utf8
Set-Content -LiteralPath (Join-Path $testDirectory 'test.c') -Value $source -Encoding utf8
$testExe = Join-Path $testDirectory 'test.exe'
# Existing kfifo contains a commented multiline macro; leave user code intact.
$compilerArgs = @('-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-comment',
    '-I', $testDirectory,
    '-I', (Join-Path $repoRoot '03_Platform/mcu_interface'),
    '-I', (Join-Path $repoRoot '03_Platform/comm'),
    '-I', (Join-Path $repoRoot '03_Platform/bsp'),
    '-I', (Join-Path $repoRoot '00_Project_Config'),
    '-I', (Join-Path $repoRoot '06_Component/Kfifo'),
    (Join-Path $testDirectory 'test.c'),
    (Join-Path $repoRoot '04_Impl/mcu_at32/at32f421_uart_adapter.c'),
    (Join-Path $repoRoot '03_Platform/bsp/bsp_rs485.c'),
    (Join-Path $repoRoot '06_Component/Kfifo/kfifo.c'), '-o', $testExe)
& $Gcc @compilerArgs
if($LASTEXITCODE -ne 0) { throw "Host build failed; sources: $testDirectory" }
& $testExe
if($LASTEXITCODE -ne 0) { throw "Host test failed; sources: $testDirectory" }
Write-Output "Host mocks only; no board timing or electrical validation. Sources: $testDirectory"
