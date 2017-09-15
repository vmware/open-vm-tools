/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVGAUTHINITIALIZER_H_
#define CVGAUTHINITIALIZER_H_


#include "IVgAuthImpersonation.h"

#include "CVgAuthContext.h"

namespace Caf {

class CVgAuthInitializer : public IVgAuthImpersonation {
public:
	CVgAuthInitializer();
	virtual ~CVgAuthInitializer();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(ICafObject)
		CAF_QI_ENTRY(IVgAuthImpersonation)
	CAF_END_QI()

public:
	void initialize(
		const std::string& applicationName);

public:
	SmartPtrCVgAuthContext getContext() const;
	void installClient() const;
	void uninstallClient() const;

public: // IVgAuthImpersonation
	void endImpersonation();

private:
	static void logHandler(
		const char *logDomain,
	    int32 logLevel,
	    const char *msg,
	    void *userData);

private:
	bool _isInitialized;
	SmartPtrCVgAuthContext _vgAuthContext;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CVgAuthInitializer);
};

CAF_DECLARE_SMART_QI_POINTER(CVgAuthInitializer);

}

#endif /* CVGAUTHINITIALIZER_H_ */
