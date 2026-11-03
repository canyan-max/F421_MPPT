param([string]$Gcc = 'D:\software_config\mingw64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $Gcc)) { throw "Compiler unavailable: $Gcc" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ('proto-local-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$source = @'
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "proto_local.h"
static unsigned g_reads, g_writes;
static proto_local_status_t g_status;
static proto_local_status_t read_registers(uint16_t address, uint16_t count,
                                           uint16_t *p_values)
{
    unsigned i;
    assert(address >= 8 && (uint32_t)address + count <= 22);
    g_reads++;
    for(i=0; i<count; i++) p_values[i]=(uint16_t)(0x8000U + address + i);
    return g_status;
}
static proto_local_status_t write_register(uint16_t address, uint16_t value)
{
    assert(address==8 && value==0x1234);
    g_writes++;
    return g_status;
}
/* Independent reference plus known check values verify CRC and byte order. */
static uint16_t reference_crc(const uint8_t *p, unsigned n)
{
    unsigned i, bit;
    uint16_t crc=0xFFFF;
    for(i=0;i<n;i++) for(bit=0;bit<8;bit++) {
        unsigned carry=(crc ^ (p[i] >> bit)) & 1;
        crc >>= 1;
        if(carry) crc ^= 0xA001;
    }
    return crc;
}
static void checksum(uint8_t *p, unsigned n)
{
    uint16_t crc=reference_crc(p,n-2);
    p[n-2]=(uint8_t)crc; p[n-1]=(uint8_t)(crc>>8);
}
static void request(uint8_t *p,uint8_t function,uint16_t address,uint16_t value)
{
    p[0]=0x15; p[1]=function; p[2]=(uint8_t)(address>>8); p[3]=(uint8_t)address;
    p[4]=(uint8_t)(value>>8); p[5]=(uint8_t)value; checksum(p,8);
}
static void exception(const uint8_t *p,unsigned length,uint8_t function,uint8_t code)
{
    assert(length==5 && p[0]==0x15 && p[1]==(function|0x80) && p[2]==code);
    assert(reference_crc(p,length)==0);
}
int main(void)
{
    const proto_local_provider_t read_only={read_registers,NULL};
    const proto_local_provider_t writable={read_registers,write_register};
    uint8_t q[257], out[260];
    unsigned address,count,capacity,length,i,before;
    uint16_t result;
    const uint8_t known[]={1,3,0,0,0,10,0xC5,0xCD};
    assert(proto_local_crc16((const uint8_t *)"123456789",9)==0x4B37);
    assert(proto_local_crc16(known,6)==0xCDC5);
    assert(proto_local_crc16(known,8)==0);
    assert(proto_local_crc16(known,0)==0xFFFF);
    assert(proto_local_crc16(NULL,8)==0);
    /* Every valid read range and each output capacity through full response. */
    for(address=8;address<=21;address++) for(count=1;count<=22-address;count++) {
        request(q,3,(uint16_t)address,(uint16_t)count);
        for(capacity=0;capacity<=5+2*count;capacity++) {
            memset(out,0xA6,sizeof(out)); before=g_reads;
            result=proto_local_handle(q,8,&read_only,out+1,(uint16_t)capacity);
            assert(out[0]==0xA6 && out[capacity+1]==0xA6);
            if(capacity<5+2*count) { assert(result==0 && before==g_reads); continue; }
            assert(result==5+2*count && out[3]==2*count && g_reads==before+1);
            assert(reference_crc(out+1,result)==0);
            for(i=0;i<count;i++) assert(out[4+2*i]==0x80 && out[5+2*i]==address+i);
        }
    }
    request(q,3,8,14); assert(q[6]==0x46 && q[7]==0xD8);
    assert(proto_local_handle(NULL,8,&read_only,out,40)==0);
    assert(proto_local_handle(q,8,&read_only,NULL,40)==0);
    q[6]^=1; before=g_reads;
    assert(proto_local_handle(q,8,&read_only,out,40)==0 && g_reads==before);
    q[6]^=1;
    q[0]=0; checksum(q,8);
    assert(proto_local_handle(q,8,&read_only,out,40)==0);
    q[0]=0x16; checksum(q,8);
    assert(proto_local_handle(q,8,&read_only,out,40)==0);
    request(q,3,8,0); exception(out,proto_local_handle(q,8,&read_only,out,40),3,3);
    request(q,3,8,126); exception(out,proto_local_handle(q,8,&read_only,out,40),3,3);
    request(q,3,7,1); exception(out,proto_local_handle(q,8,&read_only,out,40),3,2);
    request(q,3,21,2); exception(out,proto_local_handle(q,8,&read_only,out,40),3,2);
    request(q,3,0xFFFF,2); exception(out,proto_local_handle(q,8,&read_only,out,40),3,2);
    request(q,3,8,1); exception(out,proto_local_handle(q,8,NULL,out,40),3,4);
    g_status=PROTO_LOCAL_DEVICE_FAILURE;
    exception(out,proto_local_handle(q,8,&read_only,out,40),3,4);
    g_status=(proto_local_status_t)99;
    exception(out,proto_local_handle(q,8,&read_only,out,40),3,4);
    g_status=PROTO_LOCAL_OK;
    request(q,6,8,0x1234);
    exception(out,proto_local_handle(q,8,&read_only,out,40),6,2);
    assert(g_writes==0);
    for(capacity=0;capacity<8;capacity++) {
        assert(proto_local_handle(q,8,&writable,out,(uint16_t)capacity)==0);
        assert(g_writes==0);
    }
    assert(proto_local_handle(q,8,&writable,out,8)==8 && g_writes==1);
    assert(memcmp(q,out,8)==0);
    g_status=PROTO_LOCAL_ILLEGAL_VALUE;
    exception(out,proto_local_handle(q,8,&writable,out,40),6,3);
    q[0]=0; checksum(q,8); before=g_writes;
    assert(proto_local_handle(q,8,&writable,out,40)==0 && g_writes==before);
    request(q,0x10,8,1);
    exception(out,proto_local_handle(q,8,&read_only,out,40),0x10,1);
    /* Unknown functions need not have the 03/06 fixed request length. */
    q[0]=0x15; q[1]=0x11; checksum(q,4);
    exception(out,proto_local_handle(q,4,&read_only,out,40),0x11,1);
    q[1]=0x83; checksum(q,4);
    assert(proto_local_handle(q,4,&read_only,out,40)==0);
    /* Known functions require exactly eight bytes, even with a valid CRC. */
    before=g_reads;
    for(length=0;length<=257;length++) if(length!=8) {
        memset(q,0,sizeof(q)); q[0]=0x15; q[1]=3;
        if(length>=4) checksum(q,length);
        assert(proto_local_handle(q,(uint16_t)length,&read_only,out,40)==0);
        assert(g_reads==before);
    }
    puts("Local protocol: CRC, read ranges/capacities, malformed frames, exceptions and write callbacks passed.");
    return 0;
}
'@
Set-Content -LiteralPath (Join-Path $testDirectory 'test.c') -Value $source
$testExe = Join-Path $testDirectory 'test.exe'
& $Gcc -std=c99 -O2 -Wall -Wextra -Werror `
    -I (Join-Path $repoRoot '06_Component/Protocols') `
    (Join-Path $testDirectory 'test.c') `
    (Join-Path $repoRoot '06_Component/Protocols/proto_local.c') -o $testExe
if($LASTEXITCODE -ne 0) { throw "Host build failed: $testDirectory" }
& $testExe
if($LASTEXITCODE -ne 0) { throw "Host test failed: $testDirectory" }
Write-Output "Protocol-only host test; no board IO. Sources: $testDirectory"
