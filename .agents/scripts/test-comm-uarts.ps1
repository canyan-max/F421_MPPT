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
#include "bsp_uart2.h"
#include "svc_comm.h"
#include "svc_measure.h"
#include "plat_uart.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
usart_type g_test_uart;
usart_type g_test_uart2;
static uint32_t g_now, g_mask, g_reads, g_sent;
static uint32_t g_uart2_sent, g_gpio_writes;
static uint8_t g_wire[512], g_wire2[512];
static svc_measure_value_t g_value = {24000U,14400U,5000U,6230U,-1230,250};
platform_err_t svc_measure_get(svc_measure_value_t *p)
{ *p=g_value; return PLATFORM_ERR_OK; }
static plat_gpio_state_t g_direction;
uint32_t plat_tick_get_ms(void) { return g_now; }
uint32_t plat_irq_save(void) { uint32_t old=g_mask; g_mask=1U; return old; }
void plat_irq_restore(uint32_t mask) { g_mask=mask; }
platform_err_t plat_gpio_write(plat_gpio_id_t id, plat_gpio_state_t state)
{ assert(id == BOARD_GPIO_RS485_TXEN);
  g_gpio_writes++; g_direction=state; return PLATFORM_ERR_OK; }
void usart_interrupt_enable(usart_type *p, uint32_t bit, uint8_t enable)
{ if(enable) p->enabled |= bit; else p->enabled &= ~bit; }
void usart_flag_clear(usart_type *p, uint32_t flags)
{ if(flags & 15U) p->sts &= ~(15U|USART_RDBF_FLAG); else p->sts &= ~flags; }
uint16_t usart_data_receive(usart_type *p)
{ g_reads++; p->sts &= ~(15U|USART_RDBF_FLAG); return p->dt; }
void usart_data_transmit(usart_type *p, uint16_t value)
{ if(p==USART1) { assert(g_sent < sizeof(g_wire)); g_wire[g_sent++]=(uint8_t)value; }
  else { assert(p==USART2); assert(g_uart2_sent<sizeof(g_wire2)); g_wire2[g_uart2_sent++]=(uint8_t)value; }
  p->sts &= ~(USART_TDC_FLAG|USART_TDBE_FLAG); }
uint8_t usart_interrupt_flag_get(usart_type *p, uint32_t bit)
{ return (p->enabled & bit) && (p->sts & bit); }

static void rx(plat_uart_id_t id, uint8_t byte, uint32_t error)
{ usart_type *p=id==BOARD_UART_RS485?USART1:USART2;
  p->dt=byte; p->sts|=USART_RDBF_FLAG|error; plat_uart_irq_handler(id); }
static void flag(plat_uart_id_t id, uint32_t flags)
{ usart_type *p=id==BOARD_UART_RS485?USART1:USART2;
  p->sts|=flags; plat_uart_irq_handler(id); }
static void ticks(unsigned n)
{ while(n--) { g_now++; bsp_rs485_tick_1ms(); bsp_uart2_tick_1ms(); } }
static void query(plat_uart_id_t id, uint8_t cmd)
{ const uint8_t q[]={0xDD,0xA5,cmd,0,0xFF,(uint8_t)(0U-cmd),0x77};
  unsigned i; for(i=0;i<sizeof(q);i++) rx(id,q[i],0); }
static unsigned be(const uint8_t *p) { return ((unsigned)p[0]<<8)|p[1]; }
int main(void)
{
 unsigned i,gpio_before;
 uint8_t data[256]; uint16_t count;
 assert(svc_comm_init()==PLATFORM_ERR_OK);
 assert(svc_comm_init()==PLATFORM_ERR_BUSY);
 query(BOARD_UART_RS485,3); query(BOARD_UART_DEBUG,15); ticks(5);
 svc_comm_process();
 assert(g_direction==PLAT_GPIO_SET && bsp_rs485_tx_get_result()==BSP_RS485_TX_BUSY);
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_BUSY && g_uart2_sent==0);
 ticks(1); svc_comm_process(); gpio_before=g_gpio_writes;
 for(i=0;i<12;i++) {
   flag(BOARD_UART_RS485,USART_TDBE_FLAG|USART_TDC_FLAG);
   flag(BOARD_UART_DEBUG,USART_TDBE_FLAG|USART_TDC_FLAG);
 }
 assert(g_uart2_sent==12 && g_wire2[0]==0xDD && g_wire2[1]==15);
 assert(be(g_wire2+5)==2400 && be(g_wire2+7)==500);
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_BUSY);
 flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_DONE);
 assert(g_gpio_writes==gpio_before && g_direction==PLAT_GPIO_SET);
 /* UART2 builds a different response while UART1 still owns its first reply. */
 g_value.battery_voltage_mv=15000; query(BOARD_UART_DEBUG,3); ticks(5);
 svc_comm_process(); assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_BUSY);
 /* Full duplex UART2 queues another query during its TX. */
 query(BOARD_UART_DEBUG,15); ticks(5);
 for(i=0;i<32;i++) {
   if(i<20) flag(BOARD_UART_RS485,USART_TDBE_FLAG);
   flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
 }
 assert(g_sent==32 && g_wire[1]==3 && be(g_wire+4)==1440);
 assert((int16_t)be(g_wire+6)==-123 && be(g_wire+27)==2981);
 assert(g_uart2_sent==44 && g_wire2[13]==3 && be(g_wire2+16)==1500);
 flag(BOARD_UART_RS485,USART_TDC_FLAG); flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
 assert(g_direction==PLAT_GPIO_RESET && g_rs485_stats.tx_segments==1);
 svc_comm_process(); assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_BUSY);
 for(i=0;i<12;i++) flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
 flag(BOARD_UART_DEBUG,USART_TDC_FLAG); assert(g_uart2_stats.tx_segments==3);
 /* RX errors/overflow discard only UART2's damaged segment. */
 rx(BOARD_UART_DEBUG,1,0); rx(BOARD_UART_DEBUG,2,USART_FERR_FLAG);
 rx(BOARD_UART_DEBUG,3,0); ticks(5);
 assert(bsp_uart2_read_segment(data,sizeof(data),&count)==BSP_UART2_RX_DROPPED);
 assert(count==0 && g_uart2_stats.rx_errors==1);
 for(i=0;i<257;i++) rx(BOARD_UART_DEBUG,(uint8_t)i,0);
 ticks(5); assert(bsp_uart2_read_segment(data,256,&count)==BSP_UART2_RX_DROPPED);
 for(i=0;i<256;i++) rx(BOARD_UART_DEBUG,(uint8_t)i,0);
 ticks(5); assert(bsp_uart2_read_segment(data,255,&count)==BSP_UART2_RX_BUFFER_SMALL);
 assert(count==256); assert(bsp_uart2_read_segment(data,256,&count)==BSP_UART2_RX_OK);
 for(i=0;i<256;i++) assert(data[i]==(uint8_t)i);
 for(i=0;i<5;i++) {rx(BOARD_UART_DEBUG,(uint8_t)i,0); ticks(5);}
 for(i=0;i<4;i++) {assert(bsp_uart2_read_segment(data,256,&count)==BSP_UART2_RX_OK);assert(count==1&&data[0]==i);}
 assert(bsp_uart2_read_segment(data,256,&count)==BSP_UART2_RX_EMPTY);
 g_now=UINT32_MAX-2U; rx(BOARD_UART_DEBUG,0x66,0); ticks(5);
 assert(bsp_uart2_read_segment(data,256,&count)==BSP_UART2_RX_OK && data[0]==0x66);
 /* Timeout stops loading, releases storage only after abort, then allows retry. */
 data[0]=0x55; assert(bsp_uart2_send_async(data,1)==PLATFORM_ERR_OK);
 flag(BOARD_UART_DEBUG,USART_TDBE_FLAG); ticks(30); bsp_uart2_process();
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_FAILED && g_uart2_stats.tx_timeouts==1);
 assert(bsp_uart2_send_async(data,1)==PLATFORM_ERR_BUSY); ticks(5);
 assert(bsp_uart2_send_async(data,1)==PLATFORM_ERR_OK);
 flag(BOARD_UART_DEBUG,USART_TDBE_FLAG); flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_DONE);
 assert(g_gpio_writes==gpio_before+1 && g_direction==PLAT_GPIO_RESET && !g_mask);
 puts("Dual UART HIK routing, independent TX buffers, full duplex RX, gap/overflow/error/wrap and timeout recovery passed.");
 return 0;
}

'@
Set-Content -LiteralPath (Join-Path $testDirectory 'at32f421_wk_config.h') -Value $vendor -Encoding utf8
Set-Content -LiteralPath (Join-Path $testDirectory 'test.c') -Value $source -Encoding utf8
$testExe = Join-Path $testDirectory 'test.exe'
# Existing kfifo contains a commented multiline macro; leave user code intact.
$compilerArgs = @('-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-comment',
    '-I', $testDirectory,
    '-I', (Join-Path $repoRoot '02_Service'),
    '-I', (Join-Path $repoRoot '06_Component/Protocols'),
    '-I', (Join-Path $repoRoot '03_Platform/mcu_interface'),
    '-I', (Join-Path $repoRoot '03_Platform/comm'),
    '-I', (Join-Path $repoRoot '03_Platform/bsp'),
    '-I', (Join-Path $repoRoot '00_Project_Config'),
    '-I', (Join-Path $repoRoot '06_Component/Kfifo'),
    (Join-Path $testDirectory 'test.c'),
    (Join-Path $repoRoot '04_Impl/mcu_at32/at32f421_uart_adapter.c'),
    (Join-Path $repoRoot '03_Platform/bsp/bsp_rs485.c'),
    (Join-Path $repoRoot '03_Platform/bsp/bsp_uart2.c'),
    (Join-Path $repoRoot '02_Service/svc_comm.c'),
    (Join-Path $repoRoot '06_Component/Protocols/proto_hik.c'),
    (Join-Path $repoRoot '06_Component/Kfifo/kfifo.c'), '-o', $testExe)
& $Gcc @compilerArgs
if($LASTEXITCODE -ne 0) { throw "Host build failed; sources: $testDirectory" }
& $testExe
if($LASTEXITCODE -ne 0) { throw "Host test failed; sources: $testDirectory" }
Write-Output "Host mocks only; no board timing or electrical validation. Sources: $testDirectory"
