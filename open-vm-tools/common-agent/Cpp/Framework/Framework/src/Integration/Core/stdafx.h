/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define INTEGRATIONCORE_LINKAGE __declspec(dllexport)
#else
	#define INTEGRATIONCORE_LINKAGE
#endif

#include <CommonDefines.h>
#include <Integration.h>

#include "Integration/Core/FileHeaders.h"
#include "Integration/Core/MessageHeaders.h"

#endif /* STDAFX_H_ */
