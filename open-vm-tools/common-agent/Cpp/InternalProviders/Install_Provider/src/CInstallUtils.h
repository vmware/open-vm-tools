/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CInstallUtils_h_
#define CInstallUtils_h_

namespace Caf {

class CInstallUtils {
public:
	typedef enum {
		MATCH_NOTEQUAL,
		MATCH_VERSION_EQUAL,
		MATCH_VERSION_LESS,
		MATCH_VERSION_GREATER
	} MATCH_STATUS;

	static MATCH_STATUS compareVersions(
		const std::string& packageVersion1,
		const std::string& packageVersion2);

private:
	CAF_CM_DECLARE_NOCREATE(CInstallUtils);
};

}

#endif // #ifndef CInstallUtils_h_
