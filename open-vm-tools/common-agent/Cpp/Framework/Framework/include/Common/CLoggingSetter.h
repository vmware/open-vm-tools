/*
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CLoggingSetter_h_
#define CLoggingSetter_h_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CLoggingSetter {
public:
	CLoggingSetter();
	virtual ~CLoggingSetter();

public:
	void initialize(const std::string& logDir);

private:
	bool _isInitialized;
	bool _remapLoggingLocation;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CLoggingSetter);
};

CAF_DECLARE_SMART_POINTER(CLoggingSetter);

}

#endif
