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
#include "svc_packdata.h"
#include "svc_measure.h"
#include "proto_local.h"
#include "plat_uart.h"
#include "plat_gpio.h"
#include "plat_sys.h"
#include "board_resources.h"
usart_type g_test_uart;
usart_type g_test_uart2;
static uint32_t g_now, g_mask, g_reads, g_sent;
static uint32_t g_uart2_sent, g_gpio_writes;
static uint8_t g_wire[512], g_wire2[512];
static uint8_t g_measure_available = 1U;
static svc_measure_value_t g_value = {24000U,14400U,5000U,6230U,-1230,250};
platform_err_t svc_measure_get(svc_measure_value_t *p)
{ if(!g_measure_available) return PLATFORM_ERR_BUSY; *p=g_value; return PLATFORM_ERR_OK; }
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
static void local_frame(uint8_t *p, uint8_t function, uint16_t address, uint16_t value)
{
 uint16_t crc;
 p[0]=0x15; p[1]=function; p[2]=(uint8_t)(address>>8); p[3]=(uint8_t)address;
 p[4]=(uint8_t)(value>>8); p[5]=(uint8_t)value;
 crc=proto_local_crc16(p,6); p[6]=(uint8_t)crc; p[7]=(uint8_t)(crc>>8);
}
static void local_query(plat_uart_id_t id, uint8_t function, uint16_t address, uint16_t value)
{
 uint8_t q[8]; unsigned i;
 local_frame(q,function,address,value);
 for(i=0;i<8;i++) rx(id,q[i],0);
}
static void local_mapping(void)
{
 uint8_t q[8], reply[40]; unsigned i;
 svc_measure_value_t saved=g_value;
 local_frame(q,3,8,14);
 assert(svc_packdata_handle(q,8,reply,sizeof(reply))==33);
 assert(reply[0]==0x15 && reply[1]==3 && reply[2]==28);
 assert(be(reply+3)==24000 && be(reply+5)==5000);
 assert(be(reply+7)==14400 && (int16_t)be(reply+9)==-1230);
 assert(be(reply+11)==14400 && be(reply+13)==6230 && be(reply+15)==250);
 for(i=17;i<31;i++) assert(reply[i]==0);
 assert(proto_local_crc16(reply,33)==0);
 g_value.input_voltage_mv=UINT32_MAX; g_value.input_current_ma=UINT32_MAX;
 g_value.battery_voltage_mv=UINT32_MAX; g_value.output_current_ma=UINT32_MAX;
 g_value.battery_current_ma=INT32_MIN; g_value.temperature_deci_c=-125;
 assert(svc_packdata_handle(q,8,reply,sizeof(reply))==33);
 assert(be(reply+3)==65535 && be(reply+5)==65535 && be(reply+7)==65535);
 assert((int16_t)be(reply+9)==INT16_MIN && be(reply+11)==65535 && be(reply+13)==65535);
 assert((int16_t)be(reply+15)==-125);
 g_value.battery_current_ma=INT32_MAX;
 g_value.temperature_deci_c=SVC_MEASURE_TEMPERATURE_INVALID;
 assert(svc_packdata_handle(q,8,reply,sizeof(reply))==33);
 assert(be(reply+9)==INT16_MAX && be(reply+15)==0);
 g_measure_available=0;
 assert(svc_packdata_handle(q,8,reply,sizeof(reply))==5 && reply[1]==0x83 && reply[2]==4);
 g_measure_available=1; g_value=saved;
 local_frame(q,6,8,0x1234);
 assert(svc_packdata_handle(q,8,reply,sizeof(reply))==5 && reply[1]==0x86 && reply[2]==2);
 assert(proto_local_crc16(reply,5)==0);
}
int main(void)
{
 unsigned i,gpio_before;
 uint8_t data[256]; uint16_t count;
 local_mapping();
 assert(svc_comm_init()==PLATFORM_ERR_OK);
 assert(svc_comm_init()==PLATFORM_ERR_BUSY);
 /* Protocol table dispatch rejects empty/unknown/bad queries and no data. */
 {
   uint8_t q[]={0xDD,0xA5,3,0,0xFF,0xFD,0x77};
   memset(data,0xA6,sizeof(data));
   assert(svc_packdata_handle(NULL,7,data,40)==0);
   assert(svc_packdata_handle(q,0,data,40)==0);
   assert(svc_packdata_handle(q,7,NULL,40)==0);
   assert(svc_packdata_handle(q,7,data,0)==0);
   q[0]=0x08; assert(svc_packdata_handle(q,7,data,40)==0);
   q[0]=0xDD; q[5]++; assert(svc_packdata_handle(q,7,data,40)==0);
   q[5]--; assert(svc_packdata_handle(q,7,data,29)==0);
   assert(data[0]==0xA6);
   g_measure_available=0;
   assert(svc_packdata_handle(q,7,data,40)==0);
   g_measure_available=1;
   assert(svc_packdata_handle(q,7,data,40)==32);
   assert(data[32]==0xA6);
   /* Unknown prefix, invalid checksum, unsupported command: neither port sends. */
   rx(BOARD_UART_DEBUG,0x08,0); ticks(5); svc_comm_process();
   for(i=0;i<7;i++) rx(BOARD_UART_DEBUG,q[i]^(i==5?1:0),0);
   ticks(5); svc_comm_process();
   query(BOARD_UART_DEBUG,0x0C); ticks(5); svc_comm_process();
   assert(g_uart2_sent==0 && g_sent==0);
   assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_IDLE);
 }
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
 /* During abort cooldown, comm retains one encoded reply across BUSY. */
 assert(bsp_uart2_send_async(data,1)==PLATFORM_ERR_OK);
 query(BOARD_UART_DEBUG,3); ticks(30); svc_comm_process();
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_FAILED);
 g_value.battery_voltage_mv=17000;
 gpio_before=g_uart2_sent;
 ticks(5); svc_comm_process();
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_BUSY);
 for(i=0;i<32;i++) flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
 flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
 assert(g_uart2_sent==gpio_before+32 && be(g_wire2+gpio_before+4)==1500);
 svc_comm_process();
 assert(bsp_uart2_tx_get_result()==BSP_UART2_TX_DONE);
 /* Local RTU and HIK coexist on both ports; keep the first response intact. */
 g_value.battery_voltage_mv=14400;
 {
   unsigned start1=g_sent, start2=g_uart2_sent;
   uint8_t q[8];
   local_query(BOARD_UART_RS485,3,8,14); query(BOARD_UART_DEBUG,15);
   ticks(5); svc_comm_process(); ticks(1); svc_comm_process();
   for(i=0;i<12;i++) {
     flag(BOARD_UART_RS485,USART_TDBE_FLAG);
     flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
   }
   flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
   assert(g_wire2[start2]==0xDD && g_uart2_sent==start2+12);
   /* UART2 builds local response while UART1 still transmits its local one. */
   g_value.battery_voltage_mv=16000;
   local_query(BOARD_UART_DEBUG,3,10,2); ticks(5); svc_comm_process();
   for(i=0;i<21;i++) {
     flag(BOARD_UART_RS485,USART_TDBE_FLAG);
     if(i<9) flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
   }
   flag(BOARD_UART_RS485,USART_TDC_FLAG); flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
   assert(g_sent==start1+33 && g_wire[start1]==0x15);
   assert(be(g_wire+start1+7)==14400 && proto_local_crc16(g_wire+start1,33)==0);
   assert(g_uart2_sent==start2+21 && g_wire2[start2+12]==0x15);
   assert(be(g_wire2+start2+15)==16000 && proto_local_crc16(g_wire2+start2+12,9)==0);
   /* Read-only write request returns the RTU exception via the source port. */
   start1=g_sent; start2=g_uart2_sent;
   local_query(BOARD_UART_RS485,6,8,123); local_query(BOARD_UART_DEBUG,6,8,123);
   ticks(5); svc_comm_process(); ticks(1); svc_comm_process();
   for(i=0;i<5;i++) {
     flag(BOARD_UART_RS485,USART_TDBE_FLAG); flag(BOARD_UART_DEBUG,USART_TDBE_FLAG);
   }
   flag(BOARD_UART_RS485,USART_TDC_FLAG); flag(BOARD_UART_DEBUG,USART_TDC_FLAG);
   assert(g_sent==start1+5 && g_wire[start1+1]==0x86 && g_wire[start1+2]==2);
   assert(g_uart2_sent==start2+5 && g_wire2[start2+1]==0x86 && g_wire2[start2+2]==2);
   assert(proto_local_crc16(g_wire+start1,5)==0 && proto_local_crc16(g_wire2+start2,5)==0);
   /* Neither bad CRC, another slave address nor broadcast produces a reply. */
   start1=g_sent; start2=g_uart2_sent;
   local_frame(q,3,8,1); q[6]^=1;
   for(i=0;i<8;i++) {rx(BOARD_UART_RS485,q[i],0);rx(BOARD_UART_DEBUG,q[i],0);}
   ticks(5); svc_comm_process();
   local_frame(q,3,8,1); q[0]=0;
   {uint16_t crc=proto_local_crc16(q,6);q[6]=(uint8_t)crc;q[7]=(uint8_t)(crc>>8);}
   for(i=0;i<8;i++) {rx(BOARD_UART_RS485,q[i],0);rx(BOARD_UART_DEBUG,q[i],0);}
   ticks(5); svc_comm_process();
   q[0]=0x16;
   {uint16_t crc=proto_local_crc16(q,6);q[6]=(uint8_t)crc;q[7]=(uint8_t)(crc>>8);}
   for(i=0;i<8;i++) {rx(BOARD_UART_RS485,q[i],0);rx(BOARD_UART_DEBUG,q[i],0);}
   ticks(5); svc_comm_process();
   assert(g_sent==start1 && g_uart2_sent==start2 && g_direction==PLAT_GPIO_RESET);
 }
 puts("Dual UART local Modbus/HIK routing, independent TX buffers, full duplex RX, gap/overflow/error/wrap and timeout recovery passed.");
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
    (Join-Path $repoRoot '02_Service/svc_packdata.c'),
    (Join-Path $repoRoot '06_Component/Protocols/proto_hik.c'),
    (Join-Path $repoRoot '06_Component/Protocols/proto_local.c'),
    (Join-Path $repoRoot '06_Component/Kfifo/kfifo.c'), '-o', $testExe)
& $Gcc @compilerArgs
if($LASTEXITCODE -ne 0) { throw "Host build failed; sources: $testDirectory" }
& $testExe
if($LASTEXITCODE -ne 0) { throw "Host test failed; sources: $testDirectory" }
Write-Output "Host mocks only; no board timing or electrical validation. Sources: $testDirectory"
