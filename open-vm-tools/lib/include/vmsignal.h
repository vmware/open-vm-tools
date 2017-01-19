/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * vmsignal.h --
 *
 *    Posix signal handling utility functions
 *
 */


#ifndef __VMSIGNAL_H__
#   define __VMSIGNAL_H__

#include <signal.h>

int Signal_SetGroupHandler(int const *signals, struct sigaction *olds, unsigned int nr, void (*handler)(int signal));
int Signal_ResetGroupHandler(int const *signals, struct sigaction const *olds, unsigned int nr);


#endif /* __VMSIGNAL_H__ */
