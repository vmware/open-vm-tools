/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

//{{CAF_SUBSYSTEM}}

#include <CommonDefines.h>
#include <DocXml.h>
#include <DocUtils.h>
#include <Integration.h>
#include <ProviderFxLink.h>

namespace Caf {
	extern const char* _sInstallPackageSpecFilename;
	extern const char* _sInstallProviderSpecFilename;
}

#include "IPackage.h"

#include "CInstallUtils.h"
#include "CPathBuilder.h"

#include "CPackageExecutor.h"
#include "CPackageInstaller.h"
#include "CProviderInstaller.h"

#include "CInstallProvider.h"

#endif // #ifndef stdafx_h_
