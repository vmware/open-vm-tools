/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/**
 * @file slashProc.h
 */

#ifndef _SLASHPROC_H_
#define _SLASHPROC_H_

#include <glib.h>
#include <net/route.h>


/*
 * Global functions
 */


EXTERN GHashTable *SlashProcNet_GetSnmp(void);
EXTERN GHashTable *SlashProcNet_GetSnmp6(void);

EXTERN GPtrArray  *SlashProcNet_GetRoute(unsigned int, unsigned short);
EXTERN void        SlashProcNet_FreeRoute(GPtrArray *);

EXTERN GPtrArray  *SlashProcNet_GetRoute6(unsigned int, unsigned int);
EXTERN void        SlashProcNet_FreeRoute6(GPtrArray *);

#endif // ifndef _SLASHPROC_H_
