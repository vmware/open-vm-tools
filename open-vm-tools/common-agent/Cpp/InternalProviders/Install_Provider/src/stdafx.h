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
#include <CafContracts.h>
#include <DocXml.h>
#include <DocUtils.h>
#include <Integration.h>
#include <ProviderFxLink.h>

namespace Caf {
	extern const char* _sInstallPackageSpecFilename;
	extern const char* _sInstallProviderSpecFilename;
}

#include "../../Install_Provider/src/IPackage.h"

#include "../../Install_Provider/src/CInstallUtils.h"
#include "../../Install_Provider/src/CPathBuilder.h"

#include "../../Install_Provider/src/CPackageExecutor.h"
#include "../../Install_Provider/src/CPackageInstaller.h"
#include "../../Install_Provider/src/CProviderInstaller.h"

#include "../../Install_Provider/src/CInstallProvider.h"

#endif // #ifndef stdafx_h_
