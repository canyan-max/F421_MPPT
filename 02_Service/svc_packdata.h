/**
 ******************************************************************************
 *@file               :   svc_packdata.h
 *@brief              :   Project protocol dispatch and data-provider binding.
 *@version            :   V1.0
 *@note               :   1 tab == 4 spaces!
 ******************************************************************************
 */

/* define to prevent recursive inclusion ------------------------------------*/
#ifndef SVC_PACKDATA_H
#define SVC_PACKDATA_H

/* includes -----------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // end of __cplusplus
/* define -------------------------------------------------------------------*/

/* typedef ------------------------------------------------------------------*/

/* variables ----------------------------------------------------------------*/

/* function  ----------------------------------------------------------------*/
/* Main context, bounded synchronous handling of one complete BSP segment.
 * Returns reply length, or 0 for unknown/invalid/unsupported queries or no
 * data. No pointer is retained; caller owns async TX storage. Add new protocol
 * rows and project data callbacks in svc_packdata.c. Request prefixes must be
 * unique; a matched prefix owns the whole segment. */
uint16_t svc_packdata_handle(const uint8_t *p_request,
                             uint16_t       length,
                             uint8_t       *p_output,
                             uint16_t       capacity);

#ifdef __cplusplus
}
#endif // end of __cplusplus

#endif /* SVC_PACKDATA_H */
/* end of file --------------------------------------------------------------*/
