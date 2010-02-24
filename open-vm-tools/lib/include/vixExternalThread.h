/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * This header defines shared thread interface that need to be included in both
 * vixSemiPublic.h and foundryThread.c
 * foundryThread.c cannot include vixSemiPublic.h due to dependency issue
 */

#ifndef _VIX_EXTERNAL_THREAD_H_
#define _VIX_EXTERNAL_THREAD_H_

typedef void (*VixThreadFuncType)(void *);
typedef void (*VixScheduleWorkFuncType)(VixThreadFuncType, void *);

typedef struct IVixThread {
   VixScheduleWorkFuncType ScheduleWorkFunc;
   VixScheduleWorkFuncType ScheduleIOFunc;
   VixScheduleWorkFuncType ScheduleDedicatedFunc;
} IVixThread;

void Vix_SetExternalThreadInterface(IVixThread *threadInt);

#endif
