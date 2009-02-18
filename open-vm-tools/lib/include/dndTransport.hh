/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * dndTransport.hh --
 *
 *      DnDTransport provides a data transportation interface for both dnd and copyPaste.
 */

#ifndef DND_TRANSPORT_HH
#define DND_TRANSPORT_HH

#include <sigc++/connection.h>
#include <sigc++/slot.h>

#include "vm_basic_types.h"

class DnDTransport
{
public:
   virtual ~DnDTransport() {};

   /* sigc signals for RecvMsg. */
   sigc::signal<void, const uint8 *, size_t> recvMsgChanged;

   virtual bool SendMsg(uint8 *msg,
                        size_t length) = 0;
};

#endif // DND_TRANSPORT_HH
