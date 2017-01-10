/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

using namespace Caf;

namespace Caf {
	const char* _sInstallPackageSpecFilename = "installPackageSpec.xml";
	const char* _sInstallProviderSpecFilename = "installProviderSpec.xml";
}

int main(int csz, char* asz[]) {
	CAF_CM_STATIC_FUNC_LOG("InstallProvider", "main");

	int rc = 0;
	try {
		CInstallProvider provider;
		rc = CProviderDriver::processProviderCommandline(provider, csz, asz);
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	rc = CAF_CM_ISEXCEPTION ? 1 : rc;
	CAF_CM_CLEAREXCEPTION;

	return rc;
}
