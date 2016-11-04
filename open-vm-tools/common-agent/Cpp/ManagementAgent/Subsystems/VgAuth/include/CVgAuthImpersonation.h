/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVgAuthImpersonation_H_
#define CVgAuthImpersonation_H_


#include "CVgAuthContext.h"
#include "CVgAuthUserHandle.h"

namespace Caf {

class CVgAuthImpersonation {
public:
	CVgAuthImpersonation();
	virtual ~CVgAuthImpersonation();

public:
	void impersonateAndManage(
		const SmartPtrCVgAuthContext& vgAuthContext,
		const SmartPtrCVgAuthUserHandle& vgAuthUserHandle);

public:
	static void beginImpersonation(
		const SmartPtrCVgAuthContext& vgAuthContext,
		const SmartPtrCVgAuthUserHandle& vgAuthUserHandle);

	static void endImpersonation(
		const SmartPtrCVgAuthContext& vgAuthContext);

private:
	void impersonateLocal(
		const SmartPtrCVgAuthContext& vgAuthContext,
		const SmartPtrCVgAuthUserHandle& vgAuthUserHandle) const;

private:
	bool _isInitialized;
	SmartPtrCVgAuthContext _vgAuthContext;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CVgAuthImpersonation);
};

CAF_DECLARE_SMART_POINTER(CVgAuthImpersonation);

}

#endif /* CVgAuthImpersonation_H_ */
