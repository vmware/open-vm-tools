/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVgAuthError_H_
#define CVgAuthError_H_

namespace Caf {

class CVgAuthError {
public:
	static void checkErrorExc(
		const VGAuthError& vgAuthError,
	    const std::string& msg);

	static void checkErrorExc(
		const VGAuthError& vgAuthError,
	    const std::string& msg,
	    const std::string& addtlInfo);

	static void checkErrorErr(
		const VGAuthError& vgAuthError,
	    const std::string& msg);

	static void checkErrorErr(
		const VGAuthError& vgAuthError,
	    const std::string& msg,
	    const std::string& addtlInfo);

	static std::string getErrorMsg(
		const VGAuthError& vgAuthError);

	static uint32 getErrorCode(
		const VGAuthError& vgAuthError);

private:
	CAF_CM_DECLARE_NOCREATE(CVgAuthError);
};

}

#endif /* CVgAuthError_H_ */
