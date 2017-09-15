/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CAFINSTALLREQUESTDOCINC_H_
#define CAFINSTALLREQUESTDOCINC_H_

namespace Caf {
	typedef enum {
		PACKAGE_OS_NONE,
		PACKAGE_OS_ALL,
		PACKAGE_OS_NIX,
		PACKAGE_OS_WIN
	} PACKAGE_OS_TYPE;
}

#include "CPackageDefnDoc.h"
#include "CMinPackageElemDoc.h"
#include "CFullPackageElemDoc.h"

#include "CInstallPackageSpecDoc.h"
#include "CInstallProviderSpecDoc.h"

#include "CGetInventoryJobDoc.h"
#include "CUninstallProviderJobDoc.h"
#include "CInstallProviderJobDoc.h"

#include "CInstallBatchDoc.h"
#include "CInstallRequestDoc.h"

#endif /* CAFINSTALLREQUESTINC_H_ */
