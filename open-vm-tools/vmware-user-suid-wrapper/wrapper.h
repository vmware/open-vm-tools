/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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

/*
 * wrapper.h --
 *
 *      Platform independent definitions for the VMware User Agent setuid
 *      wrapper.
 */

#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include <sys/types.h>

#include <stdio.h>

#include "vm_basic_types.h"
#include "vmblock.h"

#define progname                "vmware-user"
#define Error(fmt, args...)     fprintf(stderr, "%s: " fmt, progname, ##args);


/*
 * XXX Document official VMware Tools releases vs. open-vm-tools and the
 * use of the locations database in the former vs. compile-time pathing
 * in the latter.
 */
#ifdef USES_LOCATIONS_DB
#   define LOCATIONS_PATH       "/etc/vmware-tools/locations"

/*
 * Locations DB query selector.  Values in this enum are used as array
 * indexes, so any updates to this enum must follow updating
 * main.c::queryMappings.
 */

typedef enum {
   QUERY_LIBDIR = 0,    /* Ask for "LIBDIR" */
   QUERY_BINDIR,        /* Ask for "BINDIR" */
   QUERY_SBINDIR,       /* Ask for "SBINDIR" */
   QUERY_MAX            /* Upper limit -- Insert other queries above only. */
} Selector;
#else
#   ifndef VMTOOLSD_PATH
#      error This program requires either USES_LOCATIONS_DB or VMTOOLSD_PATH.
#   endif // ifndef VMTOOLSD_PATH
#endif // ifdef USES_LOCATIONS_DB


/*
 * Global functions
 */

extern Bool CompatExec(const char *, char * const [], char * const []);
extern Bool BuildExecPath(char *, size_t);

/* See above re: USES_LOCATIONS_DB. */
#ifdef USES_LOCATIONS_DB
extern Bool QueryLocationsDB(const char *, Selector, char *, size_t);
#endif // ifdef USES_LOCATIONS_DB


#endif // ifndef _WRAPPER_H_
