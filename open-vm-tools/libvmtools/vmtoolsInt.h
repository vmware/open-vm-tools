/*********************************************************
 * Copyright (C) 2010-2016,2020 VMware, Inc. All rights reserved.
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

#ifndef _VMTOOLSINT_H_
#define _VMTOOLSINT_H_

/**
 * @file vmtoolsInt.h
 *
 * Internal definitions used by the vmtools library.
 */

#include "glibUtils.h"
#include "vmware.h"
#include "vmware/tools/utils.h"

/* ************************************************************************** *
 * Internationalization.                                                      *
 * ************************************************************************** */

void
VMToolsMsgCleanup(void);

/* ************************************************************************** *
 * Logging.                                                                   *
 * ************************************************************************** */

GlibLogger *
VMToolsCreateVMXLogger(void);

/* ************************************************************************** *
 * Miscelaneous.                                                              *
 * ************************************************************************** */

gint
VMToolsAsprintf(gchar **string,
                gchar const *format,
                ...)  PRINTF_DECL(2, 3);

#endif /* _VMTOOLSINT_H_ */

