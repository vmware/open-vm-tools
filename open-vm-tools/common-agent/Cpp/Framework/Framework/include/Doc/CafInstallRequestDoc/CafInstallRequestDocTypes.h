/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAFINSTALLREQUESTDOCTYPES_H_
#define CAFINSTALLREQUESTDOCTYPES_H_

#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"

namespace Caf {
	typedef enum {
		PACKAGE_OS_NONE,
		PACKAGE_OS_ALL,
		PACKAGE_OS_NIX,
		PACKAGE_OS_WIN
	} PACKAGE_OS_TYPE;
}

#endif /* CAFINSTALLREQUESTTYPES_H_ */
