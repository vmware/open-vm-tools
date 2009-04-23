/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/* 
 * os.h --
 *
 * 	Definitions for OS-specific wrapper functions required
 *	by "vmmemctl".  This allows customers to build their own
 *	vmmemctl driver for custom versioned kernels without the
 *	need for source code. 
 */

#ifndef	OS_H
#define	OS_H

/*
 * Needs fixing for 64bit OSes
 */
#ifndef __x86_64__
#define CDECL __attribute__((cdecl, regparm(0)))
#else
#define CDECL
#endif

/*
 * Types
 */

typedef void CDECL (*os_timer_handler)(void *);
typedef int  CDECL (*os_status_handler)(char *);

/*
 * Operations
 */

extern void CDECL *os_kmalloc_nosleep(unsigned int size);
extern void CDECL os_kfree(void *obj, unsigned int size);
extern void CDECL os_yield(void);
extern void CDECL os_bzero(void *s, unsigned int n);
extern void CDECL os_memcpy(void *dest, const void *src, unsigned int size);
extern int  CDECL os_sprintf(char *str, const char *format, ...);

extern unsigned long CDECL os_addr_to_ppn(unsigned long addr);
extern unsigned long CDECL os_alloc_reserved_page(int can_sleep);
extern void          CDECL os_free_reserved_page(unsigned long page);

extern void CDECL os_timer_init(os_timer_handler handler, void *data, int period);
extern void CDECL os_timer_start(void);
extern void CDECL os_timer_stop(void);

extern void CDECL os_init(const char *name,
                          const char *name_verbose,
                          os_status_handler handler);
extern void CDECL os_cleanup(void);

extern char CDECL *os_identity(void);

extern unsigned int CDECL os_predict_max_balloon_pages(void);

extern unsigned int CDECL os_timer_hz(void);

#endif  /* OS_H */
