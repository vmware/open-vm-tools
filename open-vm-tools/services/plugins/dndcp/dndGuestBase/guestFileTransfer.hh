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
 * @guestFileTransfer.hh --
 *
 * File transfer object for guest.
 */

#ifndef GUEST_FILE_TRANSFER_HH
#define GUEST_FILE_TRANSFER_HH

#include <sigc++/trackable.h>
#include "fileTransferRpc.hh"
#include "dndCPTransport.h"

class GuestFileTransfer
   : public sigc::trackable
{
public:
   GuestFileTransfer(DnDCPTransport *transport);
   virtual ~GuestFileTransfer(void);

protected:
   FileTransferRpc *mRpc;
};

#endif // GUEST_FILE_TRANSFER_HH
