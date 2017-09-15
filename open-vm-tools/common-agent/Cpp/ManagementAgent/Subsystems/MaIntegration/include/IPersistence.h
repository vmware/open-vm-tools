/*
 *	Author: bwilliams
 *	Created: Nov 12, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaContracts_IPersistence_h_
#define _MaContracts_IPersistence_h_


#include "ICafObject.h"

#include "Doc/PersistenceDoc/CPersistenceDoc.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
IPersistence : public ICafObject {
	CAF_DECL_UUID("5b48f8ef-7023-4583-9d2d-b15ae566b0cc")

	virtual void initialize() = 0;

	virtual SmartPtrCPersistenceDoc getUpdated(
			const int32 timeout) = 0;

	virtual void update(
			const SmartPtrCPersistenceDoc& persistence) = 0;

	virtual void remove(
			const SmartPtrCPersistenceDoc& persistence) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IPersistence);

}

#endif
