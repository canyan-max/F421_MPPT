/**
 ******************************************************************************
 *@file               :   svc_comm.h
 *@brief              :   Serve HIK measurement queries over UART1 and UART2.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_COMM_H
#define SVC_COMM_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include "plat_error.h"

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Startup once after generated GPIO/UART initialization, main context only. */
/* Returns the first failure; successfully initialized ports still run.
 * Watch g_comm_channels[0/1].init_status for UART1/UART2 respectively. */
platform_err_t svc_comm_init(void);
/* Main loop, nonblocking. Owns each BSP receive consumer and separate per-port
 * TX buffers. Supports battery (03) and solar (0F) reads; other commands are
 * ignored. */
void svc_comm_process(void);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_COMM_H */
/* end of file --------------------------------------------------------------*/
