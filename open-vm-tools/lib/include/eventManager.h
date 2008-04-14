/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * eventManager.h --
 *
 *    Multi-timer manager
 *
 */


#ifndef __EVENTMANAGER_H__
#   define __EVENTMANAGER_H__


#   include "vm_basic_types.h"
#include "dbllnklst.h"


typedef Bool (*EventManager_EventHandler)(void *clientData);

typedef struct Event Event;

DblLnkLst_Links * EventManager_Init(void);
Event *EventManager_Add(DblLnkLst_Links *eventQueue, uint32 period, 
                        EventManager_EventHandler handler, void *handlerData);
void EventManager_Remove(Event *e);
int EventManager_ProcessNext(DblLnkLst_Links *eventQueue, uint64 *sleepUsecs);
void EventManager_Destroy(DblLnkLst_Links *eventQueue);


#endif /* __EVENTMANAGER_H__ */
