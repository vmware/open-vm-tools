/* **************************************************************************
 * Copyright (C) 2010 VMware, Inc. All Rights Reserved -- VMware Confidential
 * **************************************************************************/

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
