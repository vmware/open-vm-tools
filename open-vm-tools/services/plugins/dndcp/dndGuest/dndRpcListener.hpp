/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @dndRpcListener.hpp --
 *
 * Interface for objects that receive rpc send and received notifications
 * from the vmx dnd controller. These signals are used for introspection
 * during unit testing and simulation.
 */

#ifndef DND_RPC_LISTENER_HPP
#define DND_RPC_LISTENER_HPP

class DnDRpcListener
{
public:
   virtual ~DnDRpcListener() {};
   virtual void OnRpcReceived(uint32 cmd, uint32 src, uint32 session) = 0;
   virtual void OnRpcSent(uint32 cmd, uint32 dest, uint32 session) = 0;
};

#endif // DND_RPC_LISTENER_HPP
