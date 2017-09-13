/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"

using namespace Caf;

int main(int csz, char* asz[])
{
	CTestInfraProvider provider;
	return CProviderDriver::processProviderCommandline(provider, csz, asz);
}
