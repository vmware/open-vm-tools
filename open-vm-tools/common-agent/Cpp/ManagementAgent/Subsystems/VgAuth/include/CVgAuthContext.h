/*
 *	 Author: bwilliams
 *  Created: Aug 16, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVgAuthContext_H_
#define CVgAuthContext_H_

namespace Caf {

class CVgAuthContext {
public:
	CVgAuthContext();
	virtual ~CVgAuthContext();

public:
	void initialize(
		const std::string& applicationName);

	VGAuthContext* getPtr() const;

	std::string getApplicationName() const;

private:
	bool _isInitialized;
	VGAuthContext* _vgAuthContext;
	std::string _applicationName;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CVgAuthContext);
};

CAF_DECLARE_SMART_POINTER(CVgAuthContext);

}

#endif /* CVgAuthContext_H_ */
