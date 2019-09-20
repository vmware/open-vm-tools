/*********************************************************
 * Copyright (C) 2010-2018 VMware, Inc. All rights reserved.
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
 * @guestFileTransfer.cc --
 *
 * Implementation of common layer file transfer object for guest.
 */

#include "guestFileTransfer.hh"

extern "C" {
   #include "debug.h"
}

#include "hgfsServer.h"


/**
 * Create transport object and register callback.
 *
 * @param[in] transport pointer to a transport object.
 */

GuestFileTransfer::GuestFileTransfer(DnDCPTransport *transport)
   : mRpc(NULL)
{
   ASSERT(transport);
}


/**
 * Destructor.
 */
GuestFileTransfer::~GuestFileTransfer(void)
{
}
