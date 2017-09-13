/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CENVIRONMENTUTILS_H_
#define CENVIRONMENTUTILS_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CEnvironmentUtils {
public:

	static void readEnvironmentVar(const char* varname, std::string& rValue);
	static void writeEnvironmentVar(const char* varname, std::string& rValue);

private:
	CAF_CM_DECLARE_NOCREATE(CEnvironmentUtils);
};

}

#endif
