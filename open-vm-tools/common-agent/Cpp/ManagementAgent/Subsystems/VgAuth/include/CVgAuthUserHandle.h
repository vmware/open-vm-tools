/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVgAuthUserHandle_H_
#define CVgAuthUserHandle_H_


#include "CVgAuthContext.h"

namespace Caf {

class CVgAuthUserHandle {
public:
	CVgAuthUserHandle();
	virtual ~CVgAuthUserHandle();

public:
	void initialize(
		const SmartPtrCVgAuthContext& vgAuthContext,
		const std::string& signedSamlToken);

	void initialize(
		const SmartPtrCVgAuthContext& vgAuthContext,
		const std::string& signedSamlToken,
		const std::string& userName);

public:
	std::string getUserName(
		const SmartPtrCVgAuthContext& vgAuthContext) const;

	VGAuthUserHandle* getPtr() const;

private:
	bool _isInitialized;
	VGAuthUserHandle* _vgAuthUserHandle;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CVgAuthUserHandle);
};

CAF_DECLARE_SMART_POINTER(CVgAuthUserHandle);

}

#endif /* CVgAuthUserHandle_H_ */
