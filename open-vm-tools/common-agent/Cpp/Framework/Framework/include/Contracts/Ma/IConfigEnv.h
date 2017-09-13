/*
 *	Author: bwilliams
 *	Created: Nov 12, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _MaContracts_IConfigEnv_h_
#define _MaContracts_IConfigEnv_h_

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
IConfigEnv : public ICafObject {
	CAF_DECL_UUID("dea6cea4-9385-458e-b549-d05640382da6")

	virtual void initialize() = 0;

	virtual SmartPtrCPersistenceDoc getUpdated(
			const int32 timeout) = 0;

	virtual void update(
			const SmartPtrCPersistenceDoc& persistence) = 0;

	virtual void remove(
			const SmartPtrCPersistenceDoc& persistence) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IConfigEnv);

}

#endif
