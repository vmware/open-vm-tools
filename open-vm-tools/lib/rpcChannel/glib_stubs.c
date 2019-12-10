/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @file glib_stubs.c
 *
 *    rpcChannel uses glib. If anyone wants to build rpcChannel without
 *    having any dependency on glib, we need to use this stubs file.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "util.h"

#if !defined(USE_RPCI_ONLY)
#error "This file should be compiled for RPCI-only channel!"
#endif

void *g_malloc0(size_t s) { return Util_SafeCalloc(1, s); }
void *g_malloc0_n(size_t n, size_t s) { return Util_SafeCalloc(n, s); }
void g_free(void *p) { free(p); }

void g_mutex_init(GMutex *mutex) { }
void g_mutex_clear(GMutex *mutex) { }
void g_mutex_lock(GMutex *mutex) { }
void g_mutex_unlock(GMutex *mutex) { }
