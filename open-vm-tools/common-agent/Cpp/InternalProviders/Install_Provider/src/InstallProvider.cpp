/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"

using namespace Caf;

namespace Caf {
	const char* _sInstallPackageSpecFilename = "installPackageSpec.xml";
	const char* _sInstallProviderSpecFilename = "installProviderSpec.xml";
}

int main(int csz, char* asz[])
{
	CInstallProvider provider;
	return CProviderDriver::processProviderCommandline(provider, csz, asz);
}
